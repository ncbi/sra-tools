/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include "ref_regions.h"
#include "ref_walker.h"

#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <klib/out.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <vfs/manager.h>
#include <vfs/path.h>

#include <kdb/manager.h>

#include <vdb/manager.h>
#include <vdb/schema.h>

#include <align/manager.h>
#include <align/reference.h>
#include <align/iterator.h>

#include <sra/sraschema.h>

#include <stdlib.h>
#include <os-native.h>
#include <sysalloc.h>

#include <string.h>



/* ================================================================================================ */


/***************************************
    N (0x4E)  n (0x6E)  <--> 0x0
    A (0x41)  a (0x61)  <--> 0x1
    C (0x43)  c (0x63)  <--> 0x2
    M (0x4D)  m (0x6D)  <--> 0x3
    G (0x47)  g (0x67)  <--> 0x4
    R (0x52)  r (0x72)  <--> 0x5
    S (0x53)  s (0x73)  <--> 0x6
    V (0x56)  v (0x76)  <--> 0x7
    T (0x54)  t (0x74)  <--> 0x8
    W (0x57)  w (0x77)  <--> 0x9
    Y (0x59)  y (0x79)  <--> 0xA
    H (0x48)  h (0x68)  <--> 0xB
    K (0x4B)  k (0x6B)  <--> 0xC
    D (0x44)  d (0x64)  <--> 0xD
    B (0x42)  b (0x62)  <--> 0xE
    N (0x4E)  n (0x6E)  <--> 0xF
***************************************/


static char _4na_2_ascii_tab[] =
{
/*  0x0  0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
    'N', 'A', 'C', 'M', 'G', 'R', 'S', 'V', 'T', 'W', 'Y', 'H', 'K', 'D', 'B', 'N',
    'n', 'a', 'c', 'm', 'g', 'r', 's', 'v', 't', 'w', 'y', 'h', 'k', 'd', 'b', 'n'
};


static char _4na_to_ascii( INSDC_4na_bin c, bool reverse )
{
    return _4na_2_ascii_tab[ ( c & 0x0F ) | ( reverse ? 0x10 : 0 ) ];
}


/* ================================================================================================ */


struct ref_walker
{
    /* objects that stay alive during the livetime of the ref-walker */
    KDirectory * dir;
    const VDBManager * vmgr;
    VSchema * vschema;
    const AlignMgr * amgr;
    VFSManager * vfs_mgr;
    PlacementRecordExtendFuncs cb_block;
    struct skiplist * skiplist;
    
    /* options for the Reference-Iterator */
    int32_t min_mapq;
    uint32_t interest;
    uint64_t merge_diff;
    bool prepared;
    char * spot_group;

    /* manages the sources and regions requested */
    VNamelist * sources;
    BSTree regions;

    /* enter/exit reference */
    rc_t ( CC * on_enter_ref ) ( ref_walker_data * rwd );
    rc_t ( CC * on_exit_ref ) ( ref_walker_data * rwd );

    /* enter/exit reference-window */
    rc_t ( CC * on_enter_ref_window ) ( ref_walker_data * rwd );
    rc_t ( CC * on_exit_ref_window ) ( ref_walker_data * rwd );

    /* enter/exit reference-pos */
    rc_t ( CC * on_enter_ref_pos ) ( ref_walker_data * rwd );
    rc_t ( CC * on_exit_ref_pos ) ( ref_walker_data * rwd );

    /* enter/exit spot-group */
    rc_t ( CC * on_enter_spot_group ) ( ref_walker_data * rwd );
    rc_t ( CC * on_exit_spot_group ) ( ref_walker_data * rwd );

    /* alignment */
    rc_t ( CC * on_alignment ) ( ref_walker_data * rwd );

    /* callbacks for different events */
} ref_walker;


static void ref_walker_release( struct ref_walker * self )
{
    KDirectoryRelease( self->dir );
    VDBManagerRelease( self->vmgr );
    VSchemaRelease( self->vschema );
    AlignMgrRelease ( self->amgr );
    VFSManagerRelease ( self->vfs_mgr );
    VNamelistRelease ( self->sources );
    free_ref_regions( &self->regions );
    free( ( void * )self->spot_group );
    if ( self->skiplist != NULL ) skiplist_release( self->skiplist );
}


/* ================================================================================================ */
/* data/callbacks used by Reference-iterator */


typedef struct walker_col_ids
{
    uint32_t idx_quality;
    uint32_t idx_ref_orientation;
    uint32_t idx_read_filter;
    uint32_t idx_template_len;
} walker_col_ids;


typedef struct walker_rec
{
    bool reverse;   /* orientation towards reference ( false...in ref-orientation / true...reverse) */
    int32_t tlen;   /* template-len, for statistical analysis */
    uint8_t * quality;  /* ptr to quality... ( for sam-output ) */
} walker_rec;


static rc_t read_base_and_len( struct VCursor const *curs,
                               uint32_t column_idx,
                               int64_t row_id,
                               const void ** base,
                               uint32_t * len )
{
    uint32_t elem_bits, boff, len_intern;
    const void * ptr;
    rc_t rc = VCursorCellDataDirect ( curs, row_id, column_idx, &elem_bits, &ptr, &boff, &len_intern );
    if ( rc != 0 )
    {
        LOGERR( klogInt, rc, "VCursorCellDataDirect() failed" );
    }
    else
    {
        if ( len != NULL ) *len = len_intern;
        if ( base != NULL ) *base = ptr;
    }
    return rc;
}


static rc_t CC populate_data( void *obj, const PlacementRecord *placement,
        struct VCursor const *curs, INSDC_coord_zero ref_window_start, INSDC_coord_len ref_window_len,
        void *data, void * placement_ctx )
{
    walker_rec * rec = obj;
    struct ref_walker * walker = data;
    walker_col_ids * col_ids = placement_ctx;
    rc_t rc = 0;

    rec->quality = NULL;
    if ( !( walker->interest & RW_INTEREST_DUPS ) )
    {
        const uint8_t * read_filter;
        uint32_t read_filter_len;
        rc = read_base_and_len( curs, col_ids->idx_read_filter, placement->id,
                                (const void **)&read_filter, &read_filter_len );
        if ( rc == 0 )
        {
            if ( ( *read_filter == SRA_READ_FILTER_REJECT )||
                 ( *read_filter == SRA_READ_FILTER_CRITERIA ) )
            {
                rc = RC( rcAlign, rcType, rcAccessing, rcId, rcIgnored );
            }
        }
    }

    if ( rc == 0 )
    {
        const bool * orientation;
        rc = read_base_and_len( curs, col_ids->idx_ref_orientation, placement->id,
                                (const void **)&orientation, NULL );
        if ( rc == 0 )
            rec->reverse = *orientation;
    }

    if ( rc == 0 && ( walker->interest & RW_INTEREST_QUAL ) )
    {
        const uint8_t * quality;
        uint32_t quality_len;

        rc = read_base_and_len( curs, col_ids->idx_quality, placement->id,
                                (const void **)&quality, &quality_len );
        if ( rc == 0 )
        {
            rec->quality = ( uint8_t * )rec;
            rec->quality += sizeof ( * rec );
            memcpy( rec->quality, quality, quality_len );
        }
    }

    if ( rc == 0 && ( walker->interest & RW_INTEREST_TLEN ) )
    {
        const int32_t * tlen;
        uint32_t tlen_len;

        rc = read_base_and_len( curs, col_ids->idx_template_len, placement->id,
                                (const void **)&tlen, &tlen_len );
        if ( rc == 0 && tlen_len > 0 )
            rec->tlen = *tlen;
        else
            rec->tlen = 0;
    }
    else
        rec->tlen = 0;

    return rc;
}


static rc_t CC alloc_size( struct VCursor const *curs, int64_t row_id, size_t * size,
                           void *data, void * placement_ctx )
{
    rc_t rc = 0;
    walker_rec * rec;
    struct ref_walker * walker = data;
    walker_col_ids * col_ids = placement_ctx;
    *size = ( sizeof *rec );

    if ( walker->interest & RW_INTEREST_QUAL )
    {
        uint32_t q_len;
        rc = read_base_and_len( curs, col_ids->idx_quality, row_id, NULL, &q_len );
        if ( rc == 0 )
            *size += q_len;
    }
    return rc;
}


/* ================================================================================================ */


static rc_t ref_walker_init( struct ref_walker * self )
{
    rc_t rc = KDirectoryNativeDir( &self->dir );
    if ( rc == 0 )
        rc = VDBManagerMakeRead ( &self->vmgr, self->dir );
    if ( rc == 0 )
        rc = VDBManagerMakeSRASchema( self->vmgr, &self->vschema );
    if ( rc == 0 )
        rc = AlignMgrMakeRead ( &self->amgr );
    if ( rc == 0 )
        rc =  VFSManagerMake ( &self->vfs_mgr );        
    if ( rc == 0 )
        rc = VNamelistMake ( &self->sources, 10 );

    self->cb_block.data = self;
    self->cb_block.destroy = NULL;
    self->cb_block.populate = populate_data;
    self->cb_block.alloc_size = alloc_size;
    self->cb_block.fixed_size = 0;

    BSTreeInit( &self->regions );
    self->interest = RW_INTEREST_PRIM;
    self->skiplist = 0;
    self->merge_diff = 0;
    
    if ( rc != 0 )
        ref_walker_release( self );
    return rc;
}


/* ================================================================================================ */


rc_t ref_walker_create( struct ref_walker ** self )
{
    rc_t rc;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        struct ref_walker * o = calloc( 1, sizeof **self );
        *self = NULL;
        if ( o == NULL )
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        else
        {
            rc = ref_walker_init( o );
            if ( rc == 0 )
            {
                *self = o;
            }
            else
                free( ( void * )o );
        }
    }
    return rc;
}


/* ================================================================================================ */


rc_t ref_walker_set_min_mapq( struct ref_walker * self, int32_t min_mapq )
{
    if ( self == NULL )
        return RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
    self->min_mapq = min_mapq;
    return 0;
}


rc_t ref_walker_set_spot_group( struct ref_walker * self, const char * spot_group )
{
    if ( self == NULL )
        return RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
    self->spot_group = string_dup ( spot_group, string_size( spot_group ) );
    return 0;
}
    

rc_t ref_walker_set_merge_diff( struct ref_walker * self, uint64_t merge_diff )
{
    if ( self == NULL )
        return RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
    self->merge_diff = merge_diff;
    return 0;
}

rc_t ref_walker_set_interest( struct ref_walker * self, uint32_t interest )
{
    if ( self == NULL )
        return RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
    self->interest = interest;
    return 0;
}


rc_t ref_walker_get_interest( struct ref_walker * self, uint32_t * interest )
{
    if ( self == NULL )
        return RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
    if ( interest == NULL )
        return RC( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    *interest = self->interest;
    return 0;
}

/* ================================================================================================ */


rc_t ref_walker_set_callbacks( struct ref_walker * self, ref_walker_callbacks * callbacks )
{
    if ( self == NULL )
        return RC( rcApp, rcNoTarg, rcAccessing, rcSelf, rcNull );
    if ( callbacks == NULL )
        return RC( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );

    self->on_enter_ref = callbacks->on_enter_ref;
    self->on_exit_ref = callbacks->on_exit_ref;
    self->on_enter_ref_window = callbacks->on_enter_ref_window;
    self->on_exit_ref_window = callbacks->on_exit_ref_window;
    self->on_enter_ref_pos = callbacks->on_enter_ref_pos;
    self->on_exit_ref_pos = callbacks->on_exit_ref_pos;
    self->on_enter_spot_group = callbacks->on_enter_spot_group;
    self->on_exit_spot_group = callbacks->on_exit_spot_group;
    self->on_alignment = callbacks->on_alignment;

    return 0;
}


/* ================================================================================================ */


rc_t ref_walker_add_source( struct ref_walker * self, const char * src )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
        rc = VNamelistAppend ( self->sources, src );
    return rc;
}


rc_t ref_walker_parse_and_add_range( struct ref_walker * self, const char * range )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
        rc = parse_and_add_region( &self->regions, range );
    return rc;
}


rc_t ref_walker_add_range( struct ref_walker * self, const char * name, const uint64_t start, const uint64_t end )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
        rc = add_region( &self->regions, name, start, end );
    return rc;
}


static rc_t ref_walker_prepare_1_src( struct ref_walker * self, const char * name )
{
    VPath * path = NULL;
    const VPath * local_cache = NULL;
    const KFile * remote_file = NULL;
    rc_t rc = VFSManagerResolveSpec ( self->vfs_mgr, name, &path, &remote_file, &local_cache, true );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc, "cannot resolve '$(n)' via VFSManager", "n=%s", name ) );
    }
    else
    {
        char buffer[ 4096 ];
        size_t num_read;
        rc = VPathReadPath ( path, buffer, sizeof buffer, &num_read );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc, "cannot read path from vpath for '$(n)'", "n=%s", name ) );
        }
        else
        {
            if ( rc == 0 )
            {
                int path_type = ( VDBManagerPathType ( self->vmgr, "%s", buffer ) & ~ kptAlias );
                if ( path_type == kptDatabase )
                {
                    const ReferenceList * reflist;
                    uint32_t options = ( ereferencelist_usePrimaryIds | 
                                         ereferencelist_useSecondaryIds |
                                         ereferencelist_useEvidenceIds );
                    rc = ReferenceList_MakePath( &reflist, self->vmgr, name, options, 0, NULL, 0 );
                    if ( rc != 0 )
                    {
                        PLOGERR( klogErr, ( klogErr, rc, "cannot create ReferenceList for '$(n)'", "n=%s", name ) );
                    }
                    else
                    {
                        uint32_t count;
                        rc = ReferenceList_Count( reflist, &count );
                        if ( rc != 0 )
                        {
                            PLOGERR( klogErr, ( klogErr, rc, "ReferenceList_Count() for '$(n)' failed", "n=%s", name ) );
                        }
                        else
                        {
                            uint32_t idx;
                            for ( idx = 0; idx < count && rc == 0; ++idx )
                            {
                                const ReferenceObj * refobj;
                                rc = ReferenceList_Get( reflist, &refobj, idx );
                                if ( rc != 0 )
                                {
                                    LOGERR( klogInt, rc, "ReferenceList_Get() failed" );
                                }
                                else
                                {
                                    const char * seqid;
                                    rc = ReferenceObj_SeqId( refobj, &seqid );
                                    if ( rc == 0 )
                                    {
                                        INSDC_coord_len seqlen;
                                        rc = ReferenceObj_SeqLength( refobj, &seqlen );
                                        if ( rc == 0 )
                                        {
                                            rc = add_region( &self->regions, seqid, 0, seqlen - 1 );
                                        }
                                    }
                                    ReferenceObj_Release( refobj );
                                }
                            }
                        }
                        ReferenceList_Release( reflist );
                    }
                }
            }
        }
        KFileRelease( remote_file );
        VPathRelease ( local_cache );
        VPathRelease ( path );
    }
    return rc;
}


static rc_t ref_walker_prepare( struct ref_walker * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        uint32_t s_count = 0;
        rc = VNameListCount ( self->sources, &s_count );
        if ( rc == 0 && s_count > 0 )
        {
            uint32_t r_count = count_ref_regions( &self->regions );
            if ( r_count == 0 )
            {
                uint32_t idx;
                for ( idx = 0; idx < s_count && rc == 0; ++idx )
                {
                    const char * name = NULL;
                    rc = VNameListGet ( self->sources, idx, &name );
                    if ( rc == 0 && name != NULL )
                        rc = ref_walker_prepare_1_src( self, name );
                }
            }
        }
        check_ref_regions( &self->regions, self->merge_diff );
        if ( self->merge_diff > 0 )
            self->skiplist = skiplist_make( &self->regions );
        self->prepared = ( rc == 0 );
    }
    return rc;
}


static uint32_t ref_walker_make_reflist_options( struct ref_walker * self )
{
    uint32_t res = ereferencelist_4na;

    if ( self->interest & RW_INTEREST_PRIM )
        res |= ereferencelist_usePrimaryIds;

    if ( self->interest & RW_INTEREST_SEC )
        res |= ereferencelist_useSecondaryIds;

    if ( self->interest & RW_INTEREST_EV )
        res |= ereferencelist_useEvidenceIds;

    return res;
}


static rc_t make_cursor_ids( Vector * cursor_id_vector, walker_col_ids ** cursor_ids )
{
    rc_t rc;
    walker_col_ids * ids = malloc( sizeof * ids );
    if ( ids == NULL )
        rc = RC ( rcApp, rcNoTarg, rcOpening, rcMemory, rcExhausted );
    else
    {
        rc = VectorAppend ( cursor_id_vector, NULL, ids );
        if ( rc != 0 )
            free( ids );
        else
            *cursor_ids = ids;
    }
    return rc;
}

#define COL_QUALITY "QUALITY"
#define COL_REF_ORIENTATION "REF_ORIENTATION"
#define COL_READ_FILTER "READ_FILTER"
#define COL_TEMPLATE_LEN "TEMPLATE_LEN"

static rc_t add_required_columns( struct ref_walker * self, const VTable *tbl, const VCursor ** cursor,
                                  walker_col_ids * cursor_ids )
{
    rc_t rc = VTableCreateCursorRead ( tbl, cursor );
    if ( rc != 0 )
    {
        LOGERR( klogInt, rc, "VTableCreateCursorRead() failed" );
    }

    if ( rc == 0 && ( self->interest & RW_INTEREST_QUAL ) )
    {
        rc = VCursorAddColumn ( *cursor, &cursor_ids->idx_quality, COL_QUALITY );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "VCursorAddColumn(QUALITY) failed" );
        }
    }

    if ( rc == 0 )
    {
        rc = VCursorAddColumn ( *cursor, &cursor_ids->idx_ref_orientation, COL_REF_ORIENTATION );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "VCursorAddColumn(REF_ORIENTATION) failed" );
        }
    }

    if ( rc == 0 )
    {
        rc = VCursorAddColumn ( *cursor, &cursor_ids->idx_read_filter, COL_READ_FILTER );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "VCursorAddColumn(READ_FILTER) failed" );
        }
    }

    if ( rc == 0 && ( self->interest & RW_INTEREST_TLEN ) )
    {
        rc = VCursorAddColumn ( *cursor, &cursor_ids->idx_template_len, COL_TEMPLATE_LEN );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "VCursorAddColumn(TEMPLATE_LEN) failed" );
        }
    }
    return rc;
}


static rc_t ref_walker_add_iterator( struct ref_walker * self, const char * ref_name,
                        uint64_t start, uint64_t end, const char * src_name, Vector * cur_id_vector,
                        const VDatabase *db, const ReferenceObj * ref_obj, ReferenceIterator * ref_iter,
                        const char * table_name, align_id_src id_selector )
{
    walker_col_ids * cursor_ids;
    rc_t rc = make_cursor_ids( cur_id_vector, &cursor_ids );
    if ( rc == 0 )
    {
        const VTable *tbl;
        rc_t rc = VDatabaseOpenTableRead ( db, &tbl, "%s", table_name );
        if ( rc == 0 )
        {
            const VCursor *cursor;
            rc = add_required_columns( self, tbl, &cursor, cursor_ids );
            if ( rc == 0 )
            {
                rc = ReferenceIteratorAddPlacements ( ref_iter,             /* the outer ref-iter */
                                                      ref_obj,              /* the ref-obj for this chromosome */
                                                      start - 1,            /* start ( zero-based ) */
                                                      ( end - start ) + 1,  /* length */
                                                      NULL,                 /* ref-cursor */
                                                      cursor,               /* align-cursor */
                                                      id_selector,          /* which id's */
                                                      self->spot_group,     /* what read-group */
                                                      cursor_ids            /* placement-context */
                                                     );
                VCursorRelease( cursor );
            }
            VTableRelease ( tbl );
        }
    }
    return rc;
}


#define TBL_PRIM "PRIMARY_ALIGNMENT"
#define TBL_SEC  "SECONDARY_ALIGNMENT"
#define TBL_EV   "EVIDENCE_ALIGNMENT"

rc_t CC Quitting( void );

static rc_t ref_walker_walk_alignment( struct ref_walker * self,
                                       ReferenceIterator * ref_iter,
                                       const PlacementRecord * rec,
                                       ref_walker_data * rwd )
{
    rc_t rc;

    /* cast the generic record comming from the iterator into the tool-specific one */
    walker_rec * xrec = PlacementRecordCast ( rec, placementRecordExtension1 );

    /* get all the state of the ref_iter out */
    rwd->state = ReferenceIteratorState ( ref_iter, &rwd->seq_pos );
    rwd->valid = ( ( rwd->state & align_iter_invalid ) == 0 );

    rwd->first = ( ( rwd->state & align_iter_first ) == align_iter_first );
    rwd->last  = ( ( rwd->state & align_iter_last ) == align_iter_last );

    rwd->match = ( ( rwd->state & align_iter_match ) == align_iter_match );
    rwd->skip  = ( ( rwd->state & align_iter_skip ) == align_iter_skip );

    rwd->reverse = xrec->reverse;

    if ( self->interest & RW_INTEREST_BASE )
    {
        rwd->bin_alignment_base = ( rwd->state & 0x0F );
        if ( !rwd->match )
            rwd->ascii_alignment_base = _4na_to_ascii( rwd->state, rwd->reverse );
        else
            rwd->ascii_alignment_base = rwd->ascii_ref_base;
    }

    if ( self->interest & RW_INTEREST_QUAL )
    {
        if ( rwd->skip )
            rwd->quality = ( xrec->quality[ rwd->seq_pos + 1 ] + 33 );
        else
            rwd->quality = ( xrec->quality[ rwd->seq_pos ] + 33 );
    }

    rwd->mapq = rec->mapq;

    if ( self->interest & RW_INTEREST_INDEL )
    {
        rwd->ins   = ( ( rwd->state & align_iter_insert ) == align_iter_insert );
        rwd->del   = ( ( rwd->state & align_iter_delete ) == align_iter_delete );

        if ( rwd->ins )
            rwd->ins_bases_count = ReferenceIteratorBasesInserted ( ref_iter, &rwd->ins_bases );
        if ( rwd->del )
            rwd->del_bases_count = ReferenceIteratorBasesDeleted ( ref_iter, &rwd->del_ref_pos, &rwd->del_bases );
    }

    if ( self->interest & RW_INTEREST_DEBUG )
    {
        rwd->alignment_id = rec->id;
        rwd->alignment_start_pos = rec->pos;
        rwd->alignment_len = rec->len;
    }

    rc = self->on_alignment( rwd );

    if ( self->interest & RW_INTEREST_INDEL && rwd->del && ( rwd->del_bases_count > 0 ) )
        free( ( void * )rwd->del_bases );

    return rc;
}


/* free all cursor-ids-blocks created in parallel with the alignment-cursor */
static void CC cur_id_vector_entry_whack( void *item, void *data )
{
    walker_col_ids * ids = item;
    free( ids );
}

static rc_t ref_walker_walk_ref_range( struct ref_walker * self, ref_walker_data * rwd )
{
    ReferenceIterator * ref_iter;
    rc_t rc = AlignMgrMakeReferenceIterator ( self->amgr, &ref_iter, &self->cb_block, self->min_mapq ); /* align/iterator.h */
    if ( rc == 0 )
    {
        /* construct the reference iterator */

        uint32_t idx, count;
        uint32_t reflist_options = ref_walker_make_reflist_options( self ); /* above */
        Vector cur_id_vector;
        VectorInit ( &cur_id_vector, 0, 12 );

        rc = VNameListCount ( self->sources, &count );
        for ( idx = 0; idx < count && rc == 0; ++idx )
        {
            const char * src_name = NULL;
            rc = VNameListGet ( self->sources, idx, &src_name );
            if ( rc == 0 && src_name != NULL )
            {
                const VDatabase *db;
                rc = VDBManagerOpenDBRead ( self->vmgr, &db, self->vschema, "%s", src_name );
                if ( rc == 0 )
                {
                    const ReferenceList * ref_list;
                    rc = ReferenceList_MakeDatabase( &ref_list, db, reflist_options, 0, NULL, 0 );
                    if ( rc == 0 )
                    {
                        const ReferenceObj * ref_obj;
                        rc = ReferenceList_Find( ref_list, &ref_obj, rwd->ref_name, string_size( rwd->ref_name ) );
                        if ( rc == 0 )
                        {
                            INSDC_coord_len len;
                            rc = ReferenceObj_SeqLength( ref_obj, &len );
                            if ( rc == 0 )
                            {
                                if ( rwd->ref_start == 0 )
                                    rwd->ref_start = 1;
                                if ( ( rwd->ref_end == 0 )||( rwd->ref_end > len + 1 ) )
                                    rwd->ref_end = ( len - rwd->ref_start ) + 1;

                                if ( self->interest & RW_INTEREST_PRIM )
                                    rc = ref_walker_add_iterator( self, rwd->ref_name, rwd->ref_start, rwd->ref_end, src_name, 
                                            &cur_id_vector, db, ref_obj, ref_iter, TBL_PRIM, primary_align_ids );

                                if ( rc == 0 && ( self->interest & RW_INTEREST_SEC ) )
                                    rc = ref_walker_add_iterator( self, rwd->ref_name, rwd->ref_start, rwd->ref_end, src_name, 
                                            &cur_id_vector, db, ref_obj, ref_iter, TBL_SEC, secondary_align_ids );

                                if ( rc == 0 && ( self->interest & RW_INTEREST_EV ) )
                                    rc = ref_walker_add_iterator( self, rwd->ref_name, rwd->ref_start, rwd->ref_end, src_name, 
                                            &cur_id_vector, db, ref_obj, ref_iter, TBL_EV, evidence_align_ids );

                            }
                            ReferenceObj_Release( ref_obj );
                        }
                        ReferenceList_Release( ref_list );
                    }
                    VDatabaseRelease( db );
                }
            }
        }

        if ( rc == 0 )
        {
            /* walk the reference iterator */
            
            /* because in this strategy, each ref-iter contains only 1 ref-obj, no need for a loop */
            struct ReferenceObj const * ref_obj;
            rc = ReferenceIteratorNextReference( ref_iter, NULL, NULL, &ref_obj );
            if ( rc == 0 && ref_obj != NULL )
            {

                if ( self->interest & RW_INTEREST_SEQNAME )
                    rc = ReferenceObj_Name( ref_obj, &rwd->ref_name );
                else
                    rc = ReferenceObj_SeqId( ref_obj, &rwd->ref_name );

                if ( rc == 0 )
                {
                    INSDC_coord_zero first_pos;
                    INSDC_coord_len len;
                    rc_t rc_w = 0, rc_p;
                    while ( rc == 0 && rc_w == 0 )
                    {
                        rc_w = ReferenceIteratorNextWindow ( ref_iter, &first_pos, &len );
                        if ( rc_w == 0 )
                        {
                            rc_p = 0;
                            while( rc == 0 && rc_p == 0 )
                            {
                                rc_p = ReferenceIteratorNextPos ( ref_iter, ( self->interest & RW_INTEREST_SKIP ) );
                                if ( rc_p == 0 )
                                {
                                    rc = ReferenceIteratorPosition ( ref_iter, &rwd->pos, &rwd->depth, &rwd->bin_ref_base );
                                    if ( rwd->depth > 0 && rc == 0 )
                                    {
                                        rc_t rc_sg = 0;
                                        bool skip = false;

                                        if ( self->skiplist != NULL )
                                            skip = skiplist_is_skip_position( self->skiplist, rwd->pos + 1 );

                                        if ( !skip )
                                        {
                                            rwd->ascii_ref_base = _4na_to_ascii( rwd->bin_ref_base, false );
                                            if ( self->on_enter_ref_pos != NULL )
                                                rc = self->on_enter_ref_pos( rwd );

                                            while ( rc_sg == 0 && rc == 0 )
                                            {
                                                rc_sg = ReferenceIteratorNextSpotGroup ( ref_iter, &rwd->spot_group, &rwd->spot_group_len );
                                                if ( rc_sg == 0 )
                                                {
                                                    rc_t rc_pr = 0;
                                                    if ( self->on_enter_spot_group != NULL )
                                                        rc = self->on_enter_spot_group( rwd );

                                                    while ( rc == 0 && rc_pr == 0 )
                                                    {
                                                        const PlacementRecord * rec;
                                                        rc_pr = ReferenceIteratorNextPlacement ( ref_iter, &rec );
                                                        if ( rc_pr == 0 && self->on_alignment != NULL )
                                                            rc = ref_walker_walk_alignment( self, ref_iter, rec, rwd );
                                                    }

                                                    if ( self->on_exit_spot_group != NULL )
                                                        rc = self->on_exit_spot_group( rwd );
                                                }
                                            }
                                            if ( self->on_exit_ref_pos != NULL )
                                                rc = self->on_exit_ref_pos( rwd );
                                        }
                                    }
                                    rc = Quitting();
                                }
                            }
                        }
                    }
                }
            }
        }
        ReferenceIteratorRelease ( ref_iter );

        /* free cur_id_vector */
        VectorWhack ( &cur_id_vector, cur_id_vector_entry_whack, NULL );
    }
    return rc;
}


static rc_t ref_walker_walk_ref_region( struct ref_walker * self, 
                        const struct reference_region * region, ref_walker_data * rwd )
{
    rc_t rc = 0;
    uint32_t idx, count = get_ref_node_range_count( region );
    rwd->ref_name = get_ref_node_name( region );
    
    rwd->ref_start = 0;
    rwd->ref_end = 0;
    rwd->pos = 0;
    rwd->depth = 0;
    rwd->bin_ref_base = 0;
    rwd->ascii_ref_base = 0;
    rwd->spot_group = NULL;
    rwd->spot_group_len = 0;
    rwd->state = 0;
    rwd->reverse = 0;
    
    if ( self->on_enter_ref != NULL )
        rc = self->on_enter_ref( rwd );

    if ( rc == 0 )
    {
        if ( self->skiplist != NULL )
            skiplist_enter_ref( self->skiplist, rwd->ref_name );

        for ( idx = 0; idx < count; ++ idx )
        {
            const struct reference_range * range = get_ref_range( region, idx );
            if ( range != NULL )
            {
                rwd->ref_start = get_ref_range_start( range );
                rwd->ref_end = get_ref_range_end( range );
                if ( self->on_enter_ref_window != NULL )
                    rc = self->on_enter_ref_window( rwd );
                if ( rc == 0 )
                {
                    rc = ref_walker_walk_ref_range( self, rwd );
                    if ( rc == 0 && self->on_exit_ref_window != NULL )
                        rc = self->on_exit_ref_window( rwd );
                }
                rwd->ref_start = 0;
                rwd->ref_end = 0;
            }
        }
        
        if ( self->on_exit_ref != NULL )
            rc = self->on_exit_ref( rwd );
    }
    rwd->ref_name = NULL;

    return rc;
}


rc_t ref_walker_walk( struct ref_walker * self, void * data )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        if ( !self->prepared )
            rc = ref_walker_prepare( self );

        if ( rc == 0 && self->prepared )
        {
            const struct reference_region * region = get_first_ref_node( &self->regions );
            while ( region != NULL && rc == 0 )
            {
                ref_walker_data rwd;    /* this record will be passed to all the enter/exit callback's */
                rwd.data = data;

                rc = ref_walker_walk_ref_region( self, region, &rwd );
                if ( rc == 0 )
                    region = get_next_ref_node( region );
            }
        }
    }
    return rc;
}


rc_t ref_walker_destroy( struct ref_walker * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        ref_walker_release( self );
        free( ( void * ) self );
    }
    return rc;
}

