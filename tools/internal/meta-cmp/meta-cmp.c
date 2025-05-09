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

const char UsageDefaultName[] = "meta-cmp";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s <src1> <src2> <node-path> [table]\n"
        "\n", progname );
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;
    if ( args == NULL ) {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    } else {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }
    if ( rc != 0 ) { progname = fullpath = UsageDefaultName; }
    UsageSummary ( progname );
    KOutMsg ( "Options:\n" );
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );
    return rc;
}

struct cmp_ctx {
    const char * src1;
    const char * src2;
    const char * node_path;
    const char * table;
};

static rc_t init_ctx( struct cmp_ctx * ctx, const Args * args )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    ctx -> src1 = NULL;
    ctx -> src2 = NULL;
    ctx -> node_path = NULL;
    ctx -> table = NULL;
    if ( 0 != rc ) {
        LOGERR ( klogInt, rc, "ArgsParamCount() failed" );
    } else if ( count < 3 ) {
        rc = RC( rcExe, rcNoTarg, rcReading, rcParam, rcIncorrect );
        LOGERR ( klogInt, rc, "This tool needs 3 arguments: source1, source2 and node-path" );
    }  else {
        rc = ArgsParamValue( args, 0, (const void **)&( ctx -> src1 ) );
        if ( 0 != rc ) {
            LOGERR ( klogInt, rc, "ArgsParamValue( 1 ) failed" );
        } else {
            rc = ArgsParamValue( args, 1, (const void **)&( ctx -> src2 ) );
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

static rc_t print_ctx( const struct cmp_ctx * ctx ) {
    rc_t rc = KOutMsg( "comparing meta-nodes: \n" );
    if ( 0 == rc ) { rc = KOutMsg( "\tsrc1 = %s\n", ctx -> src1 ); }
    if ( 0 == rc ) { rc = KOutMsg( "\tsrc2 = %s\n", ctx -> src2 ); }
    if ( 0 == rc ) { rc = KOutMsg( "\tnode = %s\n", ctx -> node_path ); }
    if ( 0 == rc && NULL != ctx -> table ) { rc = KOutMsg( "\ttbl  = %s\n", ctx -> table ); }
    return rc;
}

struct cmp_src {
    const VDatabase * db;
    const VTable * tbl;
};

static rc_t open_src( const VDBManager * mgr, struct cmp_src * src, const char * acc_or_path ) {
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

static void close_src( struct cmp_src * src ) {
    if ( NULL != src -> db ) {
        VDatabaseRelease( src -> db );
    } else if ( NULL != src -> tbl ) {
        VTableRelease( src -> tbl );
    }
}

static bool both_are_tbl( const struct cmp_src * src1, const struct cmp_src * src2 ) {
    return ( NULL != src1 -> tbl && NULL != src2 -> tbl );
}

static bool both_are_db( const struct cmp_src * src1, const struct cmp_src * src2 ) {
    return ( NULL != src1 -> db && NULL != src2 -> db );
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

static rc_t compare_tbl( const struct cmp_ctx * ctx, const VTable * tbl1, const VTable * tbl2,
                         const char * tbl_name ) {
    bool equal = false;
    rc_t rc = VTableMetaCompare( tbl1, tbl2, ctx -> node_path, &equal );
    if ( 0 != rc ) {
        if ( NULL == tbl_name ) {
            LOGERR ( klogInt, rc, "Table - comparison failed" );
        } else {
            PLOGERR( klogInt, ( klogInt, rc, "Table - comparison failed for table '$(tbl)'",
                                "tbl=%s", tbl_name ) );
        }
    } else {
        if ( equal ) {
            if ( NULL == tbl_name ) {
                rc = KOutMsg( "\tthe node(s) in both tables are equal\n" );
            } else {
                rc = KOutMsg( "\tthe node(s) in both '%s'-tables are equal\n", tbl_name );
            }
        } else {
            if ( NULL == tbl_name ) {
                rc = KOutMsg( "\tthe node(s) in both tables are NOT equal\n" );
            } else {
                rc = KOutMsg( "\tthe node(s) in both '%s'-tables are NOT equal\n", tbl_name );
            }
            if ( 0 == rc ) {
                cleanup_rc();
                rc = RC( rcExe, rcNoTarg, rcComparing, rcData, rcInconsistent );
            }
        }
    }
    return rc;
}

static rc_t compare_one_tbl_in_db( const struct cmp_ctx * ctx,
            const VDatabase * db1, const VDatabase * db2, const char * tbl_name ) {
    const VTable * tbl1;
    rc_t rc = VDatabaseOpenTableRead( db1, &tbl1, tbl_name );
    if ( 0 != rc ) {
        PLOGERR( klogInt, ( klogInt, rc, "cannot open table '$(tbl)' in database $(acc)",
                            "tbl=%s,acc=%s", tbl_name, ctx ->src1 ) );
    } else {
        const VTable * tbl2;
        rc = VDatabaseOpenTableRead( db2, &tbl2, tbl_name );
        if ( 0 != rc ) {
            PLOGERR( klogInt, ( klogInt, rc, "cannot open table '$(tbl)' in database $(acc)",
                                "tbl=%s,acc=%s", tbl_name, ctx ->src2 ) );
        } else {
            rc = compare_tbl( ctx, tbl1, tbl2, tbl_name );
            VTableRelease( tbl2 );
        }
        VTableRelease( tbl1 );
    }
    return rc;
}

static rc_t compare_all_tables_in_db( const struct cmp_ctx * ctx,
            const VDatabase * db1, const VDatabase * db2 ) {
    KNamelist * tables_1;
    rc_t rc = VDatabaseListTbl( db1, &tables_1 );
    if ( 0 != rc ) {
        PLOGERR( klogInt, ( klogInt, rc, "cannot enumerate tables for database $(acc)",
                            "acc=%s", ctx -> src1 ) );
    } else {
        KNamelist * tables_2;
        rc = VDatabaseListTbl( db2, &tables_2 );
        if ( 0 != rc ) {
            PLOGERR( klogInt, ( klogInt, rc, "cannot enumerate tables for database $(acc)",
                                "acc=%s", ctx -> src2 ) );
        } else {
            uint32_t count;
            rc = KNamelistCount( tables_1, &count );
            if ( 0 != rc ) {
                PLOGERR( klogInt, ( klogInt, rc, "cannot get count of tables in database $(acc)",
                                    "acc=%s", ctx -> src1 ) );
            } else {
                uint32_t idx;
                for ( idx = 0; 0 == rc && idx < count; ++idx ) {
                    const char * tbl_name;
                    rc = KNamelistGet( tables_1, idx, &tbl_name );
                    if ( 0 != rc ) {
                        PLOGERR( klogInt, ( klogInt, rc, "cannot get table-name #$(rn) from database $(acc)",
                                            "nr=%u,acc=%s", idx, ctx -> src1 ) );
                    } else {
                        if ( KNamelistContains( tables_2, tbl_name ) ) {
                            rc = compare_one_tbl_in_db( ctx, db1, db2, tbl_name );
                        }
                    }
                }
            }
            KNamelistRelease( tables_2 );
        }
        KNamelistRelease( tables_1 );
    }
    return rc;
}

static rc_t compare_db( const struct cmp_ctx * ctx, const VDatabase * db1, const VDatabase * db2 ) {
    rc_t rc = KOutMsg( "\tcomparing 2 databases : " );
    if ( 0 == rc ) {
        if ( NULL == ctx -> table ) {
            rc = KOutMsg( "for each table in them\n" );
            if ( 0 == rc ) {
                rc = compare_all_tables_in_db( ctx, db1, db2 );
            }
        } else {
            rc = KOutMsg( "for table '%s'\n", ctx -> table );
            if ( 0 == rc ) {
                rc = compare_one_tbl_in_db( ctx, db1, db2, ctx -> table );
            }
        }
    }
    return rc;
}

/* --------------------------------------------------------------------------- */
static rc_t compare( const struct cmp_ctx * ctx )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( 0 != rc ) {
        LOGERR ( klogInt, rc, "KDirectoryNativeDir() failed" );
    } else {
        const VDBManager * mgr;
        rc = VDBManagerMakeRead( &mgr, dir );
        if ( rc != 0 ) {
            LOGERR ( klogInt, rc, "VDBManagerMakeRead() failed" );
        } else {
            struct cmp_src src1;
            rc = open_src( mgr, &src1, ctx -> src1 );
            if ( 0 == rc ) {
                struct cmp_src src2;
                rc = open_src( mgr, &src2, ctx -> src2 );
                if ( 0 == rc ) {
                    if ( both_are_tbl( &src1, &src2 ) ) {
                        rc = compare_tbl( ctx, src1 . tbl, src2 . tbl, NULL );
                    } else if ( both_are_db( &src1, &src2 ) ) {
                        rc = compare_db( ctx, src1 . db, src2 . db );
                    } else {
                        rc = RC( rcExe, rcNoTarg, rcComparing, rcData, rcInconsistent );
                        LOGERR ( klogInt, rc, "Table - Database - mismatch" );
                    }
                    close_src( &src2 );
                }
                close_src( &src1 );
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
    if ( 0 != rc ) {
		LOGERR ( klogInt, rc, "ArgsMakeAndHandle failed()" );
	} else {
        struct cmp_ctx ctx;
        rc = init_ctx( &ctx, args );
        if ( 0 == rc ) {
            rc = print_ctx( &ctx );
            if ( 0 == rc ) {
                rc = compare( &ctx );
            }
        }
    }
    return VDB_TERMINATE( rc );
}
