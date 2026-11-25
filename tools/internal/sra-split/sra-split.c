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

#ifndef _h_kfs_directory_
#include "kfs/directory.h"
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_kapp_main_
#include <kapp/main.h>
#endif

#include "reader.h"
#include "writer.h"
#include "err_msg.h"

#define OPTION_SRC "src"
#define OPTION_DST "dst"
#define ALIAS_SRC "s"
#define ALIAS_DST "d"

static const char * src_usage[] = { "accession to copy from", NULL };
static const char * dst_usage[] = { "new database to copy into", NULL };

OptDef MyOptions[] = {
    { OPTION_SRC, ALIAS_SRC, NULL, src_usage, 1, true, true },
    { OPTION_DST, ALIAS_DST, NULL, dst_usage, 1, true, true }
};

const char UsageDefaultName[] = "sra-split";

rc_t CC UsageSummary ( const char * progname ) {
    return KOutMsg (
        "Summary: Copies the columns Quality and Read-Filter into new database.\n"
        "\n"
        "Usage:\n"
        "  %s <src_path> <dst_path> [options]\n"
        "\n", progname );
}

rc_t CC Usage ( const Args * args ) {
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( NULL == args ) {
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    } else {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }
    if ( 0 != rc) {
        progname = fullpath = UsageDefaultName;
    }
    UsageSummary( progname );

    KOutMsg ( "Options:\n" );
    HelpOptionsStandard();

    KOutMsg ( "\n" );

    HelpVersion( fullpath, KAppVersion() );

    return rc;
}

static const char* get_str_option( const Args *args, const char *name ) {
    const char* res = NULL;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( ( 0 == rc )&&( count > 0 ) ) {
        rc = ArgsOptionValue( args, name, 0, (const void **)&res );
    }
    return res;
}

static rc_t copy_loop( p_reader_t reader, p_writer_t writer ) {
    rc_t rc = 0;
    KOutMsg( "first %lu, count %lu\n", reader -> first, reader -> count );
    while ( 0 == rc && ! reader_done( reader ) ) {
        uint8_t * quality;
        uint32_t quality_len;
        rc = reader_get_quality( reader, &quality, &quality_len );
        if ( 0 == rc ) {
            uint8_t * filter;
            uint32_t filter_len;
            rc = reader_get_filter( reader, &filter, &filter_len );
            if ( 0 == rc ) {
                int64_t row_id = reader_get_row_id( reader );
                rc = writer_write( writer, row_id, quality, quality_len, filter, filter_len );
                if ( 0 == rc ) {
                    reader_next( reader );
                }
            }
        }
    }
    return rc;
}

static rc_t perform_copy( const char * src, const char * dst ) {
    KDirectory * dir;
    VDBManager * mgr;
    reader_t reader;
    writer_t writer;
    rc_t rc = KOutMsg( "src : %s\n", src );
    if ( 0 != rc ) return rc;
    rc = KOutMsg( "dst : %s\n", dst );
    if ( 0 != rc ) return rc;
    rc = KDirectoryNativeDir( &dir );
    if ( 0 != rc ) {
        ErrMsg( "KDirectoryNativeDir() -> %R", rc );
        return rc;
    }
    rc = VDBManagerMakeUpdate( &mgr, dir );
    if ( 0 != rc ) {
        ErrMsg( "VDBManagerMakeUpdate() -> %R", rc );
        KDirectoryRelease( dir );
        return rc;
    }
    rc = reader_init( &reader, mgr, src );
    if ( 0 != rc ) {
        VDBManagerRelease( mgr );
        KDirectoryRelease( dir );
        return rc;
    }
    rc = writer_init( &writer, mgr, dst );
    if ( 0 != rc ) {
        reader_destroy( &reader );
        VDBManagerRelease( mgr );
        KDirectoryRelease( dir );
        return rc;
    }
    /* ==================================== */
    rc = copy_loop( &reader, &writer );
    /* ==================================== */
    writer_destroy( &writer );
    reader_destroy( &reader );
    VDBManagerRelease( mgr );
    KDirectoryRelease( dir );
    KOutMsg( "Done.\n" );
    return rc;
}

/***************************************************************************
 *    Main:
 ***************************************************************************/
MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE( argc, argv, VDB_INIT_FAILED );

    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                                  MyOptions, sizeof MyOptions / sizeof ( OptDef ) );
    if ( 0 != rc ) {
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    } else {
        const char * src = get_str_option( args, OPTION_SRC );
        const char * dst = get_str_option( args, OPTION_DST );
        rc = perform_copy( src, dst );
        ArgsWhack ( args );
    }
    return VDB_TERMINATE( rc );
}
