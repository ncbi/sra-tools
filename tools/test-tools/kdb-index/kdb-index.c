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
#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/index.h>
#include <klib/text.h>
#include <klib/out.h>
#include <klib/rc.h>

#include <string.h>
#include <stdio.h>

static bool keys_only;
static const char * app_name;

static
rc_t run_idx ( const KIndex * idx, const char * spec )
{
    rc_t rc = 0;
    uint64_t span;
    int64_t id, start_id;

    for ( id = 1; ; id = start_id + span )
    {
        size_t ksize;
        char key [ 256 ];

        rc = KIndexProjectText ( idx, id, & start_id, & span, key, sizeof key, & ksize );
        if ( rc != 0 )
        {
            if ( GetRCState ( rc ) == rcNotFound && GetRCObject ( rc ) == rcId )
                rc = 0;
            break;
        }

        if ( keys_only )
            KOutMsg ( "%s\n", key );
        else
            KOutMsg ( "'%s': start = %'ld, span = %'lu\n", key, start_id, span );

#if 0
        /* just for the fun of it, now try to look up the key */
        {
            int64_t start2;
            uint64_t count2;
            rc_t rc2 = KIndexFindText ( idx, key, & start2, & count2, NULL, NULL );
            if ( rc2 != 0 )
                fprintf ( stderr, "failed to find key '%s' in index\n", key );
            else
                KOutMsg ( "  key lookup returned start = %'ld, span = %'lu\n", start2, count2 );
        }
#endif
    }

    return rc;
}

static
rc_t run_tbl ( const KTable * tbl, const char * spec, const char * idx_spec )
{
    const KIndex * idx;
    rc_t rc = KTableOpenIndexRead ( tbl, & idx, "%s", idx_spec );
    if ( rc == 0 )
    {
        rc = run_idx ( idx, spec );
        KIndexRelease ( idx );
    }

    return rc;
}

static
rc_t run_db ( const KDatabase * db, const char * spec, const char * tbl_idx )
{
    rc_t rc = 0;
    const KTable * tbl;

    size_t tlen = string_size ( tbl_idx );
    const char * idx = string_chr ( tbl_idx, tlen, ':' );
    if ( idx != NULL )
    {
        tlen = idx - tbl_idx;
        ++ idx;
    }

    rc = KDatabaseOpenTableRead ( db, & tbl, "%.*s", ( uint32_t ) tlen, tbl_idx );
    if ( rc == 0 )
    {
        rc = run_tbl ( tbl, spec, idx ? idx : "ref_spec" );
        KTableRelease ( tbl );
    }

    return rc;
}

static
rc_t run ( const KDBManager * mgr, const char * spec )
{
    rc_t rc = 0;

    const KTable * tbl;
    const KDatabase * db;

    size_t slen = string_size ( spec );
    const char * tbl_idx = string_chr ( spec, slen, ':' );
    if ( tbl_idx != NULL )
    {
        slen = tbl_idx - spec;
        ++ tbl_idx;
    }

    switch ( KDBManagerPathType ( mgr, "%.*s", ( uint32_t ) slen, spec ) )
    {
    case kptDatabase:
        rc = KDBManagerOpenDBRead ( mgr, & db, "%.*s", ( uint32_t ) slen, spec );
        if ( rc == 0 )
        {
            rc = run_db ( db, spec, tbl_idx ? tbl_idx : "STATS:ref_spec" );
            KDatabaseRelease ( db );
        }
        break;
    case kptTable:
        rc = KDBManagerOpenTableRead ( mgr, & tbl, "%.*s", ( uint32_t ) slen, spec );
        if ( rc == 0 )
        {
            rc = run_tbl ( tbl, spec, tbl_idx ? tbl_idx : "ref_spec" );
            KTableRelease ( tbl );
        }
        break;
    default:
        rc = RC ( rcApp, rcMgr, rcAccessing, rcPath, rcIncorrect );
    }
    return rc;
}

static
void print_help ( void )
{
    size_t nlen = string_size ( app_name );
    const char * short_name = string_rchr ( app_name, nlen, '/' );
    if ( short_name ++ == NULL )
        short_name = app_name;

    KOutMsg ( "\n"
              "Usage:\n"
              "  %s [ options ] obj-spec [ obj-spec.. ]\n"
              "\n"
              "Object Specification:\n"
              "  <db-path-or-accession> [ ':' <table-name> [ ':' <index-name> ] ]\n"
              "  <tbl-path-or-accession> [ ':' <index-name> ]\n"
              "\n"
              "Options:\n"
              "  -k|--keys-only                   only list keys\n"
              "  -h|--help                        print this message\n"
              "\n"
              "%s : %V\n"
              "\n"
              , short_name
              , app_name
              , KAppVersion ()
        );
}

const char UsageDefaultName[] = "kdb-index";

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    rc_t rc = 0;
    int i, count = 0;
    const KDBManager * mgr;

    app_name = argv [ 0 ];

    for ( i = 1; i < argc; ++ i )
    {
        const char * arg = argv [ i ];
        if ( arg [ 0 ] != '-' )
            argv [ ++ count ] = ( char * ) arg;
        else do switch ( ( ++ arg ) [ 0 ] )
        {
        case 'k':
            keys_only = true;
            break;
        case 'h':
        case '?':
            print_help ();
            return 0;
        case 'V':
            HelpVersion ( UsageDefaultName, KAppVersion () );
            return 0;
        case '-':
            ++ arg;
            if ( strcmp ( arg, "keys-only" ) == 0 )
                keys_only = true;
            else if ( strcmp ( arg, "help" ) == 0 )
            {
                print_help ();
                return 0;
            }
            else
            {
                fprintf ( stderr, "%s: unrecognized switch: '--%s'\n", argv [ 0 ], arg );
                return -1;
            }
            arg = "\0";
            break;
        default:
            fprintf ( stderr, "%s: unrecognized switch: '-%c'\n", argv [ 0 ], arg [ 0 ] );
            return -1;
        }
        while ( arg [ 1 ] != 0 );
    }

    if ( count == 0 )
    {
        fprintf ( stderr, "%s: at least one object spec is required\n", argv [ 0 ] );
        return -1;
    }

    rc = KDBManagerMakeRead ( & mgr, NULL );
    if ( rc == 0 )
    {

        for ( i = 1; rc == 0 && i <= count; ++ i )
            rc = run ( mgr, argv [ i ] );

        KDBManagerRelease ( mgr );
    }

    return VDB_TERMINATE( rc );
}
