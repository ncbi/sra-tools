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
*  and reliability of the software and data", the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties", express or implied", including
*  warranties of performance", merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <klib/out.h>
#include <klib/rc.h>
#include <klib/log.h>

#include <kdb/database.h>
#include <kdb/manager.h>

#include <vdb/vdb-priv.h>
#include <vdb/manager.h>
#include <vdb/database.h>

#include <kapp/main.h>
#include <kapp/args.h>

#include <sysalloc.h>

const char UsageDefaultName[] = "pacbio-correct";

rc_t CC UsageSummary ( const char * progname )
{
    OUTMSG ( ("\n"
        "Usage:\n"
        "  %s writable_object(s)\n"
        "\n", progname) );
    return 0;
}


rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );
    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );

    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );
    return rc;
}

const char CONSENSUS[] = "CONSENSUS";
const char PULSE[] = "PULSE";
const char SEQUENCE[] = "SEQUENCE";

rc_t correct( const char * obj )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "creation of KDirectory failed" );
    }
    else
    {
        VDBManager * vdb_mgr;
        rc = VDBManagerMakeUpdate ( &vdb_mgr, dir );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "creation of VDBManager failed" );
        }
        else
        {
            VDatabase * vdb_db;
            rc = VDBManagerOpenDBUpdate ( vdb_mgr, &vdb_db, NULL, "%s", obj );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "vdb: open for update failed" );
            }
            else
            {
                KDatabase *kdb;
                rc = VDatabaseOpenKDatabaseUpdate ( vdb_db, & kdb );
                if ( rc != 0 )
                {
                    LOGERR( klogErr, rc, "kdb: open for update failed" );
                }
                else
                {
                    if ( KDatabaseExists ( kdb, kptTable, CONSENSUS ) )
                    {
                        if ( KDatabaseExists ( kdb, kptTable, PULSE ) )
                        {
                            OUTMSG(( "table >%s< does already exist in >%s<\n", PULSE, obj ));
                        }
                        else
                        {
                            if ( KDatabaseExists ( kdb, kptTable, SEQUENCE ) )
                            {
                                rc = KDatabaseRenameTable ( kdb, true, SEQUENCE, PULSE );
                                if ( rc != 0 )
                                {
                                    LOGERR( klogErr, rc, "kdb: renaming table failed" );
                                }
                                else
                                {
                                    OUTMSG(( "table >%s< renamed to >%s< in >%s<\n", SEQUENCE, PULSE, obj ));
                                }
                                if ( rc == 0 )
                                {
                                    rc = KDatabaseAliasTable ( kdb, CONSENSUS, SEQUENCE );
                                    if ( rc != 0 )
                                    {
                                        LOGERR( klogErr, rc, "kdb: creating table-alias failed" );
                                    }
                                    else
                                    {
                                        OUTMSG(( "alias >%s< created for >%s< in >%s<\n", SEQUENCE, CONSENSUS, obj ));
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        OUTMSG(( "table >%s< does not exist in >%s<\n", CONSENSUS, obj ));
                    }
                    KDatabaseRelease ( kdb );
                }
                VDatabaseRelease( vdb_db );
            }
            VDBManagerRelease( vdb_mgr );
        }
        KDirectoryRelease( dir );
    }
    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 0 );

    KLogHandlerSetStdErr();
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "error creating internal structure" );
    }
    else
    {
        uint32_t count;
        rc = ArgsParamCount( args, &count );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "ArgsParamCount failed" );
        else
        {
            if ( count < 1 )
            {
                rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
                LOGERR( klogErr, rc, "object(s) missing" );
                Usage ( args );
            }
            else
            {
                uint32_t idx;
                for ( idx = 0; idx < count && rc == 0; ++idx )
                {
                    const char *obj;
                    rc = ArgsParamValue( args, idx, (const void **)&obj );
                    if ( rc != 0 )
                        LOGERR( klogErr, rc, "error reading commandline-parameter" );
                    else
                        rc = correct( obj ); /* ** <<<<<<<<<<<<<<<<<< ** */
                }
            }
        }
        ArgsWhack ( args );
    }

    return VDB_TERMINATE( rc );
}
