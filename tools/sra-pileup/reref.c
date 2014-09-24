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

#include "reref.h"

#include <klib/text.h>
#include <klib/log.h>

#include <kfs/file.h>

#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

#include <align/reference.h>

#include <vdb/manager.h>
#include <vdb/vdb-priv.h>
#include <kdb/manager.h>

#include <sysalloc.h>
#include <stdlib.h>

#if 0

typedef struct report_row_ctx
{
    uint32_t prim_idx;
    uint32_t sec_idx;
    int64_t row_id;
} report_row_ctx;


static rc_t report_ref_row( const VCursor *cur, report_row_ctx * row_ctx )
{
    rc_t rc = 0;
    uint32_t elem_bits, boff, prim_count, sec_count;
    const void *base;
    rc = VCursorCellDataDirect ( cur, row_ctx->row_id, row_ctx->prim_idx, &elem_bits, &base, &boff, &prim_count );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot read colum >PRIMARY_ALIGNMENT_IDS<" );
    }
    else
    {
        rc = VCursorCellDataDirect ( cur, row_ctx->row_id, row_ctx->sec_idx, &elem_bits, &base, &boff, &sec_count );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot read colum >SECONDARY_ALIGNMENT_IDS<" );
        }
        else if ( prim_count > 0 || sec_count > 0 )
        {
            rc = KOutMsg( "ROW[ %,lu ]: PRIM:%,u SEC:%,u\n", row_ctx->row_id, prim_count, sec_count );
        }
    }
    return rc;
}


static rc_t report_ref_cursor( const VCursor *cur, int64_t start, int64_t stop )
{
    report_row_ctx row_ctx;
    rc_t rc = VCursorAddColumn ( cur, &row_ctx.prim_idx, "PRIMARY_ALIGNMENT_IDS" );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot add column >PRIMARY_ALIGNMENT_IDS<" );
    }
    else
    {
        rc = VCursorAddColumn ( cur, &row_ctx.sec_idx, "SECONDARY_ALIGNMENT_IDS" );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot add column >SECONDARY_ALIGNMENT_IDS<" );
        }
        else
        {
            rc = VCursorOpen ( cur );
            if ( rc != 0 )
            {
                (void)LOGERR( klogErr, rc, "cannot open REFERENCE-CURSOR" );
            }
            else
            {
                for ( row_ctx.row_id = start; rc == 0 && row_ctx.row_id <= stop; ++row_ctx.row_id )
                {
                    rc = report_ref_row( cur, &row_ctx );
                }
            }
        }
    }
    return rc;
}


static rc_t report_ref_table( const VDBManager *vdb_mgr, const char * path, int64_t start, int64_t stop )
{
    const VDatabase* db;
    rc_t rc = VDBManagerOpenDBRead ( vdb_mgr, &db, NULL, "%s", path );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot open vdb-database" );
    }
    else
    {
        const VTable* tb;
        rc = VDatabaseOpenTableRead ( db, &tb, "REFERENCE" );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot open REFERENCE-table" );
        }
        else
        {
            const VCursor *cur;
            rc = VTableCreateCursorRead ( tb, &cur );
            if ( rc != 0 )
            {
                (void)LOGERR( klogErr, rc, "cannot open REFERENCE-cursor" );
            }
            else
            {
                rc = report_ref_cursor( cur, start, stop );
                VCursorRelease( cur );
            }
            VTableRelease ( tb );
        }
        VDatabaseRelease ( db );
    }
    return rc;
}
#endif


static rc_t report_ref_table2( const ReferenceObj* ref_obj, int64_t start, int64_t stop )
{
    rc_t rc = 0;
    int64_t row_id, max_prim_at, max_sec_at, max_ev_at;
    uint64_t sum_prim, sum_sec, sum_ev, max_prim, max_sec, max_ev;
    sum_prim = sum_sec = sum_ev = max_prim = max_sec = max_ev = 0;

    for ( row_id = start; rc == 0 && row_id <= stop; ++row_id )
    {
        uint32_t count[ 3 ];
        rc = ReferenceObj_GetIdCount( ref_obj, row_id, count );
        if ( rc == 0 )
        {
            sum_prim += count[ 0 ];
            sum_sec  += count[ 1 ];
            sum_ev   += count[ 2 ];

            if ( count[ 0 ] > max_prim )
            {
                max_prim = count[ 0 ];
                max_prim_at = row_id;
            }
            if ( count[ 1 ] > max_sec )
            {
                max_sec = count[ 1 ];
                max_sec_at = row_id;
            }
            if ( count[ 2 ] > max_ev )
            {
                max_ev = count[ 2 ];
                max_ev_at = row_id;
            }
        }
    }

    rc = KOutMsg( "alignments:\t%,u PRI\t%,u SEC\t%,u EV\n", sum_prim, sum_sec, sum_ev );
    if ( rc == 0 && max_prim > 0 )
    {
        uint64_t from = ( ( max_prim_at - start ) * 5000 ) + 1;
        rc = KOutMsg( "max. PRI:\t%,u\tat row #%,i ( from pos: %,u ... %,u )\n", max_prim, max_prim_at, from, from + 4999 );
    }
    if ( rc == 0 && max_sec > 0 )
    {
        uint64_t from = ( ( max_sec_at - start ) * 5000 ) + 1;
        rc = KOutMsg( "max. SEC:\t%,u\tat row #%,i ( from pos: %,u ... %,u )\n", max_sec, max_sec_at, from, from + 4999 );
    }
    if ( rc == 0 && max_ev > 0 )
    {
        uint64_t from = ( ( max_ev_at - start ) * 5000 ) + 1;
        rc = KOutMsg( "max. EV:\t%,u\tat row #%,i ( from pos: %,u ... %,u )\n", max_ev, max_ev_at, from, from + 4999 );
    }
    return rc;
}


const char * ss_Database        = "Database";
const char * ss_Table           = "Table";
const char * ss_PrereleaseTbl   = "Prerelease Table";
const char * ss_Column          = "Column";
const char * ss_Index           = "Index";
const char * ss_NotFound        = "not found";
const char * ss_BadPath         = "bad path";
const char * ss_File            = "File";
const char * ss_Dir             = "Dir";
const char * ss_CharDev         = "CharDev";
const char * ss_BlockDev        = "BlockDev";
const char * ss_FIFO            = "FIFO";
const char * ss_ZombieFile      = "ZombieFile";
const char * ss_Dataset         = "Dataset";
const char * ss_Datatype        = "Datatype";
const char * ss_unknown         = "unknown pathtype";


static const char * path_type_2_str( const uint32_t pt )
{
    const char * res = ss_unknown;
    switch ( pt )
    {
    case kptDatabase      :   res = ss_Database; break;
    case kptTable         :   res = ss_Table; break;
    case kptPrereleaseTbl :   res = ss_PrereleaseTbl; break;
    case kptColumn        :   res = ss_Column; break;
    case kptIndex         :   res = ss_Index; break;
    case kptNotFound      :   res = ss_NotFound; break;
    case kptBadPath       :   res = ss_BadPath; break;
    case kptFile          :   res = ss_File; break;
    case kptDir           :   res = ss_Dir; break;
    case kptCharDev       :   res = ss_CharDev; break;
    case kptBlockDev      :   res = ss_BlockDev; break;
    case kptFIFO          :   res = ss_FIFO; break;
    case kptZombieFile    :   res = ss_ZombieFile; break;
    case kptDataset       :   res = ss_Dataset; break;
    case kptDatatype      :   res = ss_Datatype; break;
    default               :   res = ss_unknown; break;
    }
    return res;
}


static rc_t resolve_accession( VFSManager * vfs_mgr, const char * accession, const String ** path )
{
    VResolver * resolver;
    rc_t rc = VFSManagerGetResolver( vfs_mgr, &resolver );
    if ( rc != 0 )
        KOutMsg( "cannot get VResolver from VFSManger for '%s'\n", accession );
    else
    {
        VPath * vpath;
        rc = VFSManagerMakePath( vfs_mgr, &vpath, "ncbi-acc:%s?vdb-ctx=refseq", accession );
        if ( rc != 0 )
            KOutMsg( "cannot make VPath from VFSManger for '%s'\n", accession );
        else
        {
            const VPath * local;
            const VPath * remote;

            rc = VResolverQuery ( resolver, eProtocolHttp, vpath, &local, &remote, NULL );
            if ( rc != 0 )
                KOutMsg( "cannot resolve '%s'\n", accession );
            else
            {
                if ( local != NULL )
                    rc = VPathMakeString( local, path );
                else 
                    rc = VPathMakeString( remote, path );
                if ( local != NULL )
                    VPathRelease ( local );
                if ( remote != NULL )
                    VPathRelease ( remote );
            }
            VPathRelease ( vpath );
        }
        VResolverRelease( resolver );
    }
    return rc;
}


static rc_t report_ref_loc( const VDBManager *vdb_mgr, VFSManager * vfs_mgr, const char * seq_id )
{
    const String * path;
    rc_t rc = resolve_accession( vfs_mgr, seq_id, &path );
    if ( rc == 0 )
    {
        rc = KOutMsg( "location:\t%S\n", path );
        if ( rc == 0 )
        {
            uint32_t pt = VDBManagerPathType ( vdb_mgr, "%S", path );
            const char * spt = path_type_2_str( pt );
            rc = KOutMsg( "pathtype:\t%s\n", spt );
        }
        free ( (void*) path );
    }
    return rc;
}


static rc_t report_ref_obj( const VDBManager *vdb_mgr, VFSManager * vfs_mgr, const char * path, uint32_t idx,
                            const ReferenceObj* ref_obj, bool extended )
{
    const char * s;
    const char * seq_id;
    INSDC_coord_len len;
    bool circular, external;
    int64_t start, stop;

    rc_t rc = ReferenceObj_Name( ref_obj, &s );
    if ( rc == 0 )
        rc = KOutMsg( "\nREF[%u].Name     = '%s'\n", idx, s );

    if ( rc == 0 )
        rc = ReferenceObj_SeqId( ref_obj, &seq_id );
    if ( rc == 0 )
        rc = KOutMsg( "REF[%u].SeqId    = '%s'\n", idx, seq_id );

    if ( rc == 0 )
        rc = ReferenceObj_SeqLength( ref_obj, &len );
    if ( rc == 0 )
        rc = KOutMsg( "REF[%u].Length   = %,u\n", idx, len );

    if ( rc == 0 )
        rc = ReferenceObj_Circular( ref_obj, &circular );
    if ( rc == 0 )
        rc = KOutMsg( "REF[%u].Circular = %s\n", idx, circular ? "yes" : "no" );

    if ( rc == 0 )
        rc = ReferenceObj_IdRange( ref_obj, &start, &stop );
    if ( rc == 0 )
        rc = KOutMsg( "REF[%u].IdRange  = [%,lu]...[%,lu]\n", idx, start, stop );

    if ( rc == 0 )
        rc = ReferenceObj_External( ref_obj, &external, NULL );
    if ( rc == 0 )
        rc = KOutMsg( "REF[%u].Extern   = %s\n", idx, external ? "yes" : "no" );
    if ( rc == 0 && external )
    {
        rc = report_ref_loc( vdb_mgr, vfs_mgr, seq_id );
    }

    if ( rc == 0 && extended )
        rc = report_ref_table2( ref_obj, start, stop );

    return rc;
}


static rc_t report_ref_database( const VDBManager *vdb_mgr, VFSManager * vfs_mgr, const char * path, bool extended )
{
    const ReferenceList* reflist;
    uint32_t options = ( ereferencelist_usePrimaryIds | ereferencelist_useSecondaryIds | ereferencelist_useEvidenceIds );
    rc_t rc = ReferenceList_MakePath( &reflist, vdb_mgr, path, options, 0, NULL, 0 );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot create ReferenceList" );
    }
    else
    {
        uint32_t count;
        rc = ReferenceList_Count( reflist, &count );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "ReferenceList_Count() failed" );
        }
        else
        {
            rc = KOutMsg( "this object uses %u references\n", count );
            if ( rc == 0 )
            {
                uint32_t idx;
                for ( idx = 0; idx < count && rc == 0; ++idx )
                {
                    const ReferenceObj* ref_obj;
                    rc = ReferenceList_Get( reflist, &ref_obj, idx );
                    if ( rc != 0 )
                    {
                        (void)LOGERR( klogErr, rc, "ReferenceList_Get() failed" );
                    }
                    else
                    {
                        rc = report_ref_obj( vdb_mgr, vfs_mgr, path, idx, ref_obj, extended );
                        ReferenceObj_Release( ref_obj );
                    }
                }
            }
        }
        ReferenceList_Release( reflist );
    }
    return rc;
}


static rc_t report_references( const VDBManager *vdb_mgr, VFSManager * vfs_mgr, const char * spec,
                               bool extended )
{
    rc_t rc = KOutMsg( "\nreporting references of '%s'\n", spec );
    if ( rc == 0 )
    {
        VPath * path = NULL;
        const VPath * local_cache = NULL;
        const KFile * remote_file = NULL;
        rc = VFSManagerResolveSpec ( vfs_mgr, spec, &path, &remote_file, &local_cache, true );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot resolve spec via VFSManager" );
        }
        else
        {
            char buffer[ 4096 ];
            size_t num_read;
            rc = VPathReadPath ( path, buffer, sizeof buffer, &num_read );
            if ( rc != 0 )
            {
                (void)LOGERR( klogErr, rc, "cannot read path from vpath" );
            }
            else
            {
                rc = KOutMsg( "resolved into '%s'\n", buffer );
                if ( rc == 0 )
                {
                    int path_type = ( VDBManagerPathType ( vdb_mgr, "%s", buffer ) & ~ kptAlias );
                    switch( path_type )
                    {
                        case kptDatabase : rc = report_ref_database( vdb_mgr, vfs_mgr, buffer, extended );
                                           break;

                        case kptTable    : KOutMsg( "cannot report references on a table-object\n" );
                                            rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcInvalid );
                                            (void)LOGERR( klogErr, rc, "cannot report references on a table-object" );
                                           break;

                        default          : KOutMsg( "the given object is not a vdb-database\n" );
                                            rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcInvalid );
                                            (void)LOGERR( klogErr, rc, "the given object is not a vdb-database" );
                                           break;
                    }
                }
            }
            KFileRelease( remote_file );
            VPathRelease ( local_cache );
            VPathRelease ( path );
        }
    }
    return rc;
}


rc_t report_on_reference( Args * args, bool extended )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
    {
        LOGERR( klogInt, rc, "ArgsParamCount() failed" );
    }
    else
    {
        KDirectory *dir; 
        rc = KDirectoryNativeDir( &dir );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "KDirectoryNativeDir() failed" );
        }
        else
        {
            const VDBManager *vdb_mgr;
            rc = VDBManagerMakeRead ( &vdb_mgr, dir );
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "VDBManagerMakeRead() failed" );
            }
            else
            {
                VFSManager * vfs_mgr;
                rc =  VFSManagerMake ( &vfs_mgr );
                if ( rc != 0 )
                {
                    (void)LOGERR( klogErr, rc, "cannot make vfs-manager" );
                }
                else
                {
                    uint32_t idx;
                    for ( idx = 0; idx < count && rc == 0; ++idx )
                    {
                        const char *param = NULL;
                        rc = ArgsParamValue( args, idx, &param );
                        if ( rc != 0 )
                        {
                            LOGERR( klogInt, rc, "ArgsParamvalue() failed" );
                        }
                        else
                        {
                            rc = report_references( vdb_mgr, vfs_mgr, param, extended );
                        }
                    }
                    VFSManagerRelease ( vfs_mgr );
                }
                VDBManagerRelease( vdb_mgr );
            }
            KDirectoryRelease( dir );
        }
    }
    return rc;
}
