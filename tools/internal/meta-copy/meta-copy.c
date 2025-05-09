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

#include <kapp/main.h>

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/log.h>

#include <vdb/manager.h>
#include <vdb/table.h>
#include <vdb/database.h>

#include <stdlib.h>
#include <string.h>

const char UsageDefaultName[] = "meta-copy";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s <src> <dst> <node-path> [table]\n"
        "\n", progname );
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;
    if ( NULL == args ) {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    } else {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }
    if ( 0 != rc ) { progname = fullpath = UsageDefaultName; }
    UsageSummary ( progname );
    KOutMsg ( "Options:\n" );
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );
    return rc;
}

struct copy_ctx {
    const char * src;
    const char * dst;
    const char * node_path;
    const char * table;
};

static rc_t init_ctx( struct copy_ctx * ctx, const Args * args )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    ctx -> src = NULL;
    ctx -> dst = NULL;
    ctx -> node_path = NULL;
    ctx -> table = NULL;
    if ( 0 != rc ) {
        LOGERR ( klogInt, rc, "ArgsParamCount() failed" );
    } else if ( count < 3 ) {
        rc = RC( rcExe, rcNoTarg, rcReading, rcParam, rcIncorrect );
        LOGERR ( klogInt, rc, "This tool needs 3 arguments: source, destination and node-path" );
    }  else {
        rc = ArgsParamValue( args, 0, (const void **)&( ctx -> src ) );
        if ( 0 != rc ) {
            LOGERR ( klogInt, rc, "ArgsParamValue( 1 ) failed" );
        } else {
            rc = ArgsParamValue( args, 1, (const void **)&( ctx -> dst ) );
            if ( 0 != rc ) {
                LOGERR ( klogInt, rc, "ArgsParamValue( 2 ) failed" );
            } else {
                rc = ArgsParamValue( args, 2, (const void **)&( ctx -> node_path ) );
                if ( 0 != rc ) {
                    LOGERR ( klogInt, rc, "ArgsParamValue( 3 ) failed" );
                } else if ( count > 3 ) {
                    rc = ArgsParamValue( args, 3, (const void **)&( ctx -> table ) );
                    if ( 0 != rc ) {
                        LOGERR ( klogInt, rc, "ArgsParamValue( 4 ) failed" );
                    }
                }
            }
        }
    }
    return rc;
}

static rc_t print_ctx( const struct copy_ctx * ctx ) {
    rc_t rc = KOutMsg( "copying meta-nodes: \n" );
    if ( 0 == rc ) { rc = KOutMsg( "\tsrc  = %s\n", ctx -> src ); }
    if ( 0 == rc ) { rc = KOutMsg( "\tdst  = %s\n", ctx -> dst ); }
    if ( 0 == rc ) { rc = KOutMsg( "\tnode = %s\n", ctx -> node_path ); }
    if ( 0 == rc && NULL != ctx -> table ) { rc = KOutMsg( "\ttbl  = %s\n", ctx -> table ); }
    return rc;
}

struct copy_src {
    const VDatabase * db;
    const VTable * tbl;
};

struct copy_dst {
    VDatabase * db;
    VTable * tbl;
};

static rc_t open_src( const VDBManager * mgr, struct copy_src * src, const char * acc_or_path ) {
    rc_t rc = VDBManagerOpenDBRead ( mgr, &( src -> db ), NULL, "%s", acc_or_path );
    if ( 0 == rc ) {
        src -> tbl = NULL;
    } else {
        src -> db = NULL;
        rc = VDBManagerOpenTableRead( mgr, &( src -> tbl ), NULL, "%s", acc_or_path );
        if ( 0 != rc ) {
            PLOGERR( klogInt, ( klogInt, rc, "source $(acc) cannot be opened", "acc=%s", acc_or_path ) );
            src -> tbl = NULL;
        }
    }
    return rc;
}

static rc_t open_dst( VDBManager * mgr, struct copy_dst * dst, const char * acc_or_path ) {
    rc_t rc = VDBManagerOpenDBUpdate ( mgr, &( dst -> db ), NULL, "%s", acc_or_path );
    if ( 0 == rc ) {
        dst -> tbl = NULL;
    } else {
        dst -> db = NULL;
        rc = VDBManagerOpenTableUpdate( mgr, &( dst -> tbl ), NULL, "%s", acc_or_path );
        if ( 0 != rc ) {
            PLOGERR( klogInt, ( klogInt, rc, "source $(acc) cannot be opened", "acc=%s", acc_or_path ) );
            dst -> tbl = NULL;
        }
    }
    return rc;
}

static void close_src( struct copy_src * src ) {
    if ( NULL != src -> db ) {
        VDatabaseRelease( src -> db );
    } else if ( NULL != src -> tbl ) {
        VTableRelease( src -> tbl );
    }
}

static void close_dst( struct copy_dst * src ) {
    if ( NULL != src -> db ) {
        VDatabaseRelease( src -> db );
    } else if ( NULL != src -> tbl ) {
        VTableRelease( src -> tbl );
    }
}

static bool both_are_tbl( const struct copy_src * src, const struct copy_dst * dst ) {
    return ( NULL != src -> tbl && NULL != dst -> tbl );
}

static bool both_are_db( const struct copy_src * src, const struct copy_dst * dst ) {
    return ( NULL != src -> db && NULL != dst -> db );
}

static void cleanup_rc( void ) {
    rc_t rc;
    const char *filename;
    const char *funcname;
    uint32_t lineno;
    while( GetUnreadRCInfo( &rc, &filename, &funcname, &lineno ) ) {
        ;
    }
}
/* --------------------------------------------------------------------------- */

static rc_t copy_tbl( const struct copy_ctx * ctx, const VTable * src, VTable * dst,
                         const char * tbl_name ) {
    rc_t rc = VTableMetaCopy( dst, src, ctx -> node_path );
    if ( 0 != rc ) {
        if ( NULL == tbl_name ) {
            LOGERR ( klogInt, rc, "Table - copy failed" );
        } else {
            PLOGERR( klogInt, ( klogInt, rc, "Table - copy failed for table '$(tbl)'",
                                "tbl=%s", tbl_name ) );
        }
    } else {
        if ( NULL == tbl_name ) {
            rc = KOutMsg( "\tcopying the node(s) was successful\n" );
        } else {
            rc = KOutMsg( "\tcopying the node(s) in '%s'-tables was successful\n", tbl_name );
        }
    }
    return rc;
}

static rc_t copy_tbl_in_db( const struct copy_ctx * ctx,
            const VDatabase * src_db, VDatabase * dst_db, const char * tbl_name ) {
    const VTable * src_tbl;
    rc_t rc = VDatabaseOpenTableRead( src_db, &src_tbl, tbl_name );
    if ( 0 != rc ) {
        PLOGERR( klogInt, ( klogInt, rc, "cannot open table '$(tbl)' in database $(acc)",
                            "tbl=%s,acc=%s", tbl_name, ctx -> src ) );
    } else {
        VTable * dst_tbl;
        rc = VDatabaseOpenTableUpdate( dst_db, &dst_tbl, tbl_name );
        if ( 0 != rc ) {
            PLOGERR( klogInt, ( klogInt, rc, "cannot open table '$(tbl)' in database $(acc)",
                                "tbl=%s,acc=%s", tbl_name, ctx ->dst ) );
        } else {
            rc = copy_tbl( ctx, src_tbl, dst_tbl, tbl_name );
            VTableRelease( dst_tbl );
        }
        VTableRelease( src_tbl );
    }
    return rc;
}

static rc_t copy_all_tables_in_db( const struct copy_ctx * ctx,
            const VDatabase * src_db, VDatabase * dst_db ) {
    KNamelist * tables;
    rc_t rc = VDatabaseListTbl( src_db, &tables );
    if ( 0 != rc ) {
        PLOGERR( klogInt, ( klogInt, rc, "cannot enumerate tables for database $(acc)",
                            "acc=%s", ctx -> src ) );
    } else {
        uint32_t count;
        rc = KNamelistCount( tables, &count );
        if ( 0 != rc ) {
            PLOGERR( klogInt, ( klogInt, rc, "cannot get count of tables in database $(acc)",
                                "acc=%s", ctx -> src ) );
        } else {
            uint32_t idx;
            for ( idx = 0; 0 == rc && idx < count; ++idx ) {
                const char * tbl_name;
                rc = KNamelistGet( tables, idx, &tbl_name );
                if ( 0 != rc ) {
                    PLOGERR( klogInt, ( klogInt, rc, "cannot get table-name #$(rn) from database $(acc)",
                                        "nr=%u,acc=%s", idx, ctx -> src ) );
                } else {
                    rc = copy_tbl_in_db( ctx, src_db, dst_db, tbl_name );
                }
            }
        }
    }
    return rc;
}

static rc_t copy_db( const struct copy_ctx * ctx, const VDatabase * src, VDatabase * dst ) {
    rc_t rc = KOutMsg( "\tcopy 2 databases : " );
    if ( 0 == rc ) {
        if ( NULL == ctx -> table ) {
            rc = KOutMsg( "for each table in them\n" );
            if ( 0 == rc ) {
                rc = copy_all_tables_in_db( ctx, src, dst );
            }
        } else {
            rc = KOutMsg( "for table '%s'\n", ctx -> table );
            if ( 0 == rc ) {
                rc = copy_tbl_in_db( ctx, src, dst, ctx -> table );
            }
        }
    }
    return rc;
}

/* --------------------------------------------------------------------------- */
static rc_t copy( const struct copy_ctx * ctx )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( 0 != rc ) {
        LOGERR ( klogInt, rc, "KDirectoryNativeDir() failed" );
    } else {
        VDBManager * mgr;
        rc = VDBManagerMakeUpdate ( &mgr, dir );
        if ( rc != 0 ) {
            LOGERR ( klogInt, rc, "VDBManagerMakeUpdate() failed" );
        } else {
            struct copy_src src;
            rc = open_src( mgr, &src, ctx -> src );
            if ( 0 == rc ) {
                struct copy_dst dst;
                rc = open_dst( mgr, &dst, ctx -> dst );
                if ( 0 == rc ) {
                    if ( both_are_tbl( &src, &dst ) ) {
                        rc = copy_tbl( ctx, src . tbl, dst . tbl, NULL );
                    } else if ( both_are_db( &src, &dst ) ) {
                        rc = copy_db( ctx, src . db, dst . db );
                    } else {
                        rc = RC( rcExe, rcNoTarg, rcComparing, rcData, rcInconsistent );
                        LOGERR ( klogInt, rc, "Table - Database - mismatch" );
                    }
                    close_dst( &dst );
                }
                close_src( &src );
            }
            VDBManagerRelease( mgr );
        }
        KDirectoryRelease( dir );
    }
    return rc;
}

/***************************************************************************
    Main:
***************************************************************************/
MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);
    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,  NULL, 0 );
    if ( rc != 0 ) {
		LOGERR ( klogInt, rc, "ArgsMakeAndHandle failed()" );
	} else {
        struct copy_ctx ctx;
        rc = init_ctx( &ctx, args );
        if ( 0 == rc ) {
            rc = print_ctx( &ctx );
            if ( 0 == rc ) {
                rc = copy( &ctx );
            }
        }
    }
    return VDB_TERMINATE( rc );
}
