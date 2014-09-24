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

#include "ref_exclude.h"
#include "columns.h"
#include <klib/printf.h>
#include <klib/out.h>
#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>

#define MAXLEN_COLUMN   "MAX_SEQ_LEN"
#define HITS_COLUMN     "HITS"
#define HITMAP_TAB      "HITMAP"


typedef struct trans_node
{
    BSTNode node;
    String chromosome;
    String translation;
} trans_node;


static trans_node * make_trans_node( const char * chromosome, const char * translation )
{
    trans_node * res = calloc( 1, sizeof * res );
    if ( res != NULL )
    {
        StringInitCString( &(res->chromosome), chromosome );
        StringInitCString( &(res->translation), translation );
    }
    return res;
}


static void CC whack_trans_node( BSTNode *n, void *data )
{
    free( n );
}


static int CC trans_node_find( const void *item, const BSTNode *n )
{
    trans_node * node = ( trans_node * ) n;
    return StringCompare ( ( String * ) item, &node->chromosome );
}


#define N_TRANS_NODES 24

static const char * chromosomes[ N_TRANS_NODES ] =
{
    "CM000663.1",
    "CM000664.1",
    "CM000665.1",
    "CM000666.1",
    "CM000667.1",
    "CM000668.1",
    "CM000669.1",
    "CM000670.1",
    "CM000671.1",
    "CM000672.1",
    "CM000673.1",
    "CM000675.1",
    "CM000674.1",
    "CM000676.1",
    "CM000677.1",
    "CM000678.1",
    "CM000679.1",
    "CM000680.1",
    "CM000681.1",
    "CM000682.1",
    "CM000683.1",
    "CM000684.1",
    "CM000685.1",
    "CM000686.1"
};


static const char * translations[ N_TRANS_NODES ] =
{
    "NC_000001.10",
    "NC_000002.11",
    "NC_000003.11",
    "NC_000004.11",
    "NC_000005.9",
    "NC_000006.11",
    "NC_000007.13",
    "NC_000008.10",
    "NC_000009.11",
    "NC_000010.10",
    "NC_000011.9",
    "NC_000013.10",
    "NC_000012.11",
    "NC_000014.8",
    "NC_000015.9",
    "NC_000016.9",
    "NC_000017.10",
    "NC_000018.9",
    "NC_000019.9",
    "NC_000020.10",
    "NC_000021.8",
    "NC_000022.10",
    "NC_000023.10",
    "NC_000024.9"
};


static int CC trans_node_sort( const BSTNode *item, const BSTNode *n )
{
    trans_node * rn1 = ( trans_node* ) item;
    trans_node * rn2 = ( trans_node* ) n;
    return StringCompare ( &rn1->chromosome, &rn2->chromosome );
}


static rc_t insert_trans_nodes( BSTree *tree )
{
    rc_t rc = 0;
    uint32_t idx;
    for ( idx = 0; idx < N_TRANS_NODES && rc == 0; ++idx )
    {
        trans_node * node = make_trans_node( chromosomes[ idx ], translations[ idx ] );
        if ( node != NULL )
        {
            rc = BSTreeInsert ( tree, (BSTNode *)node, trans_node_sort );
        }
    }
    return rc;
}

/******************************************************************************/

typedef struct ref_node
{
    BSTNode node;
    const String *name;
    bool valid;
    const VDatabase *db;
    const VTable *tab;
    const VCursor *cur;
    uint32_t hits_idx;
    uint32_t read_len;
    uint64_t bytes_requested;
    uint64_t active_positions;
} ref_node;


static rc_t detect_read_len( ref_node *node )
{
    const VCursor *temp_cursor;
    rc_t rc = VTableCreateCursorRead ( node->tab, &temp_cursor );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc, 
             "error to create cursor on table $(tab_name)",
             "tab_name=%S", node->name ) );
    }
    else
    {
        uint32_t idx;
        rc = VCursorAddColumn ( temp_cursor, &idx, MAXLEN_COLUMN );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc, 
                 "error to add column $(col_name) to cursor for table $(db_name).$(tab_name)",
                 "col_name=%s,db_name=%S,tab_name=%s",
                 MAXLEN_COLUMN, node->name, HITMAP_TAB ) );
        }
        else
        {
            rc = VCursorOpen( temp_cursor );
            if ( rc != 0 )
            {
                PLOGERR( klogInt, ( klogInt, rc, 
                     "error to open cursor for table $(db_name).$(tab_name to read $(col_name)",
                     "col_name=%s,db_name=%S,tab_name=%s",
                     MAXLEN_COLUMN, node->name, HITMAP_TAB ) );
            }
            else
            {
                uint32_t elem_bits, boff, row_len;
                const void *base;
                rc = VCursorCellDataDirect ( temp_cursor, 1, idx,
                                             &elem_bits, &base, &boff, &row_len );
                if ( rc != 0 )
                {
                    PLOGERR( klogInt, ( klogInt, rc, 
                         "error to read $(col_name) from 1st row in table $(db_name).$(tab_name)",
                         "col_name=%s,db_name=%S,tab_name=%s",
                         MAXLEN_COLUMN, node->name, HITMAP_TAB ) );
                }
                else
                {
                    node->read_len = *((uint32_t *)base);
                    if ( node->read_len == 0 )
                    {
                        rc = RC( rcApp, rcNoTarg, rcReading, rcParam, rcInvalid );
                        PLOGERR( klogInt, ( klogInt, rc, 
                             "$(col_name) == 0 discoverd in table $(db_name).$(tab_name)",
                             "col_name=%s,db_name=%S,tab_name=%s",
                             MAXLEN_COLUMN, node->name, HITMAP_TAB ) );
                    }
                }
            }
        }
        VCursorRelease( temp_cursor );
    }
    return rc;
}


static rc_t prepare_ref_node( ref_node * node )
{
    rc_t rc = VTableCreateCursorRead ( node->tab, &node->cur );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc, 
             "error to create cursor on table $(db_name).$(tab_name)",
             "db_name=%S,tab_name=%s",
             node->name, HITMAP_TAB ) );
    }
    else
    {
        rc = VCursorAddColumn ( node->cur, &node->hits_idx, HITS_COLUMN );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc, 
                 "error to add column $(col_name) to cursor for table $(db_name).$(tab_name)",
                 "col_name=%s,db_name=%S,tab_name=%s",
                 HITS_COLUMN, node->name, HITMAP_TAB ) );
        }
        else
        {
            rc = VCursorOpen( node->cur );
            if ( rc != 0 )
            {
                PLOGERR( klogInt, ( klogInt, rc, 
                     "error to open cursor for table $(tab_name)",
                     "tab_name=%S", node->name ) );
            }
            else
            {
                node->valid = true;
            }
        }
    }
    return rc;
}


static ref_node * make_ref_node( ref_exclude *exclude, const String * s )
{
    ref_node * res = calloc( 1, sizeof * res );
    if ( res != NULL )
    {
        if ( StringCopy ( &res->name, s ) != 0 )
        {
            free( res );
            res = NULL;
        }
    }
    if ( res != NULL && exclude->path != NULL )
    {
        char buf[ 1024 ];
        size_t num_writ;
        rc_t rc = string_printf ( buf, sizeof buf, &num_writ, 
                                  "%s/%S", exclude->path, s );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc, 
                 "error to assemble path to exclude-table $(tab_name)",
                 "tab_name=%S", s ) );
        }
        else
        {
            rc = VDBManagerOpenDBRead ( exclude->mgr, &res->db, NULL, "%s", buf );
            if ( rc != 0 )
            {
                PLOGERR( klogInt, ( klogInt, rc, 
                     "error to open exclude-table $(db_name)",
                     "db_name=%s", buf ) );
                /*
                it can be OK if the database/table cannot be found!
                */
                rc = 0;
            }
            else
            {
                rc = VDatabaseOpenTableRead ( res->db, &res->tab, "HITMAP" );
                if ( rc != 0 )
                {
                    PLOGERR( klogInt, ( klogInt, rc, 
                         "error to open exclude-table 'HITMAP' in $(db_name)",
                         "db_name=%s", buf ) );
                    /*
                    it can be OK if the database/table cannot be found!
                    */
                    rc = 0;
                }
                else
                {
                    rc = detect_read_len( res );
                    if ( rc == 0 )
                    {
                        rc = prepare_ref_node( res );
                    }
                }
            }
        }
    }
    return res;
}


static rc_t read_from_ref_node( ref_node * node, 
                                int32_t ref_offset, uint32_t ref_len,
                                uint8_t *exclude_vector,
                                uint32_t *active )
{
    rc_t rc = 0;
    uint64_t row_id = ( ref_offset / node->read_len ) + 1;
    uint8_t *dst = exclude_vector;
    uint32_t remaining = ref_len;
    uint32_t src_ofs = ref_offset % node->read_len;

    while ( remaining > 0 && rc == 0 )
    {
        uint32_t elem_bits, boff, rlen;
        const uint8_t *src;
        rc = VCursorCellDataDirect ( node->cur, row_id, node->hits_idx,
                                     &elem_bits, (const void**)&src, &boff, &rlen );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc, 
                 "error to read $(col_name) from 1st row in table $(db_name).$(tab_name)",
                 "col_name=%s,db_name=%S,tab_name=%s",
                 HITS_COLUMN, node->name, HITMAP_TAB ) );
        }
        else
        {
            if ( src_ofs >= rlen )
            {
                rc = RC( rcApp, rcNoTarg, rcReading, rcParam, rcInvalid );
                PLOGERR( klogInt, ( klogInt, rc, 
                     "error: try to read more data than are in var-loc $(tab_name)",
                     "tab_name=%S", node->name ) );
            }
            else
            {
                uint32_t to_copy = ( rlen - src_ofs );
                if ( to_copy > remaining )
                {
                    to_copy = remaining;
                }
                src += src_ofs;

                memcpy( dst, src, to_copy );
                dst += to_copy;
                remaining -= to_copy;
                src_ofs = 0;
                row_id ++;

                node->bytes_requested += to_copy;
            }
        }
    }
    *active = 0;
    if ( rc == 0 )
    {
        for ( src_ofs = 0; src_ofs < ref_len; ++src_ofs )
        {
            if ( exclude_vector[ src_ofs ] > 0 )
            {
                ( *active )++;
            }
        }
    }

    return rc;
}


static void CC whack_ref_node( BSTNode *n, void *data )
{
    ref_node * node = ( ref_node * )n;
    bool * info = ( bool * )data;

    if ( *info )
    {
        OUTMSG(( "node >%S< used for %lu bytes (%lu active)\n",
                  node->name, node->bytes_requested, node->active_positions ));
    }

    if ( node->cur != NULL )
    {
        VCursorRelease( node->cur );
    }
    if ( node->tab != NULL )
    {
        VTableRelease( node->tab );
    }
    if ( node->name != NULL )
    {
        StringWhack ( node->name );
    }
    free( n );
}


static int CC ref_node_find( const void *item, const BSTNode *n )
{
    ref_node * node = ( ref_node * ) n;
    return StringCompare ( ( String * ) item, node->name );
}


static ref_node * find_ref_node( ref_exclude *exclude, const String * s )
{
    BSTNode *node;

    if ( exclude->last_used_ref_node != NULL )
    {
        ref_node * node = ( ref_node * )exclude->last_used_ref_node;
        if ( StringCompare ( s, node->name ) == 0 )
            return node;
    }

    node = BSTreeFind ( &exclude->ref_nodes, s, ref_node_find );
    if ( node == NULL )
        return NULL;
    else
    {
        exclude->last_used_ref_node = node;
        return ( ref_node * ) node;
    }
}


rc_t make_ref_exclude( ref_exclude *exclude, KDirectory *dir,
                       const char * path, bool info )
{
    rc_t rc;

    BSTreeInit( &exclude->ref_nodes );
    BSTreeInit( &exclude->translations );

    exclude->last_used_ref_node = NULL;
    exclude->info = info;
    rc = VDBManagerMakeUpdate ( &exclude->mgr, dir );
    if ( rc != 0 )
    {
        LogErr( klogInt, rc, "VDBManagerMakeUpdate() in make_ref_exclude() failed\n" );
    }
    else
    {
        insert_trans_nodes( &exclude->translations );
        exclude->path = string_dup_measure ( path, NULL );
    }
    return rc;
}


static int CC ref_node_sort( const BSTNode *item, const BSTNode *n )
{
    ref_node * rn1 = ( ref_node* ) item;
    ref_node * rn2 = ( ref_node* ) n;
    return StringCompare ( rn1->name, rn2->name );
}


static rc_t find_or_make_by_name( ref_exclude *exclude,
                          const String * name,
                          ref_node ** node )
{
    rc_t rc = 0;
    *node = find_ref_node( exclude, name );
    if ( *node == NULL )
    {
        /* if not found: make such a node... */
        *node = make_ref_node( exclude, name );
        if ( *node == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
        }
        else
        {
            /* if node was successfully made, insert it into our tree */
            rc = BSTreeInsert ( &exclude->ref_nodes, (BSTNode *)( *node ), ref_node_sort );
        }
    }
    return rc;
}

rc_t get_ref_exclude( ref_exclude *exclude,
                      const String * name,
                      int32_t ref_offset, uint32_t ref_len,
                      uint8_t *exclude_vector,
                      uint32_t *active )
{
    rc_t rc = 0;
    ref_node *node = NULL;

    /* look if we already have a node with the given name */
    trans_node *t_node = ( trans_node * )BSTreeFind ( &exclude->translations, name, trans_node_find );
    if ( t_node != NULL )
    {
        rc = find_or_make_by_name( exclude, &t_node->translation, &node );
    }
    else
    {
        rc = find_or_make_by_name( exclude, name, &node );
    }

    if ( rc == 0 && node->valid )
    {
        /* read the necessary row(s) and fill it into the exclude_vector */
        rc = read_from_ref_node( node, ref_offset, ref_len, exclude_vector, active );
        if ( rc == 0 )
        {
            node->active_positions += *active;
        }
    }
    return rc;
}


rc_t whack_ref_exclude( ref_exclude *exclude )
{
    BSTreeWhack ( &exclude->ref_nodes, whack_ref_node, &exclude->info );
    BSTreeWhack ( &exclude->translations, whack_trans_node, NULL );
    if ( exclude->mgr != NULL )
        VDBManagerRelease( exclude->mgr );
    if ( exclude->path != NULL )
        free( exclude->path );
    return 0;
}
