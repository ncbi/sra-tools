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

#include "report_deletes.h"
#include "cg_tools.h"

#include <klib/text.h>
#include <klib/log.h>
#include <klib/out.h>
#include <kfs/file.h>

#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

#include <kdb/manager.h>
#include <vdb/manager.h>
#include <vdb/vdb-priv.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <sysalloc.h>
#include <stdlib.h>

rc_t CC Quitting ( void );

static rc_t cigar_loop( const VCursor *cur,
                        uint32_t cigar_idx,
                        int64_t first,
                        uint64_t count,
                        uint32_t min_len )
{
    rc_t rc = 0;
    int64_t row_id, last_row = ( first + count );
    rna_splice_candidates candidates;

    for ( row_id = first; ( row_id < last_row ) && ( rc == 0 ) && ( Quitting() == 0 ); row_id++ )
    {
        const char * cigar;
        uint32_t row_len;
        rc = VCursorCellDataDirect ( cur, row_id, cigar_idx, NULL, ( const void ** )&cigar, NULL, &row_len );
        if ( rc == 0 )
        {
            candidates.count = 0;
            candidates.fwd_matched = 0;
            candidates.rev_matched = 0;

            rc = discover_rna_splicing_candidates( row_len, cigar, min_len, &candidates );
            if ( rc == 0 && candidates.count > 0 )
            {
                rc = KOutMsg( "%d rna-splice-candidates at row #%ld : %.*s\n", candidates.count, row_id, row_len, cigar );
            }
        }
    }
    return rc;
}


static rc_t report_deletes_db( const VDBManager *vdb_mgr,
                               const char * path,
                               uint32_t min_len )
{
    const VDatabase *db;
    rc_t rc = VDBManagerOpenDBRead( vdb_mgr, &db, NULL, "%s", path );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr,
                 ( klogInt, rc, "cannot open database $(db_name)", "db_name=%s", path ) );
    }
    else
    {
        const VTable *tab;
        rc = VDatabaseOpenTableRead( db, &tab, "PRIMARY_ALIGNMENT" );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot open table PRIMARY_ALIGNMENT" );
        }
        else
        {
            const VCursor *cur;
            rc = VTableCreateCursorRead( tab, &cur );
            if ( rc != 0 )
            {
                (void)LOGERR( klogErr, rc, "cannot open cursor on table PRIMARY_ALIGNMENT" );
            }
            else
            {
                uint32_t cigar_idx;
                rc = VCursorAddColumn( cur, &cigar_idx, "CIGAR_SHORT" );
                if ( rc != 0 )
                {
                    (void)LOGERR( klogErr, rc, "cannot add CIGAR_SHORT to cursor" );
                }
                else
                {
                    rc = VCursorOpen( cur );
                    if ( rc != 0 )
                    {
                        (void)LOGERR( klogErr, rc, "cannot open cursor" );
                    }
                    else
                    {
                        int64_t first;
                        uint64_t count;
                        rc = VCursorIdRange ( cur, cigar_idx, &first, &count );
                        if ( rc != 0 )
                        {
                            (void)LOGERR( klogErr, rc, "cannot detect row-range" );
                        }
                        else
                        {
                            rc = cigar_loop( cur, cigar_idx, first, count, min_len );
                        }
                    }
                }
                VCursorRelease( cur );
            }
            VTableRelease( tab );
        }
        VDatabaseRelease( db );
    }
    return rc;
}


static rc_t report_deletes_spec( const VDBManager *vdb_mgr,
                                 VFSManager * vfs_mgr,
                                 const char * spec,
                                 uint32_t min_len )
{
    rc_t rc = KOutMsg( "\nreporting deletes of '%s'\n", spec );
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
                int path_type = ( VDBManagerPathType ( vdb_mgr, "%s", buffer ) & ~ kptAlias );
                switch( path_type )
                {
                    case kptDatabase : rc = report_deletes_db( vdb_mgr, buffer, min_len ); break;

                    case kptTable    : KOutMsg( "cannot report deletes on a table-object\n" );
                                        rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcInvalid );
                                        (void)LOGERR( klogErr, rc, "cannot report references on a table-object" );
                                       break;

                    default          : KOutMsg( "the given object is not a vdb-database\n" );
                                        rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcInvalid );
                                        (void)LOGERR( klogErr, rc, "the given object is not a vdb-database" );
                                       break;
                }
            }
            KFileRelease( remote_file );
            VPathRelease ( local_cache );
            VPathRelease ( path );
        }
    }
    return rc;

}


rc_t report_deletes( Args * args, uint32_t min_len )
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
                            rc = report_deletes_spec( vdb_mgr, vfs_mgr, param, min_len );
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
