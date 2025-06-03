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

#include <loader/queue-file.h>

#include <kapp/main.h>
#include <kapp/args.h>

#include <klib/out.h>
#include <klib/rc.h>
#include <klib/checksum.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <sysalloc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define OPTION_POS    "pos"
#define ALIAS_POS     "p"

#define OPTION_QSIZE  "qsize"
#define ALIAS_QSIZE   "q"

#define OPTION_BSIZE  "bsize"
#define ALIAS_BSIZE   "b"

#define OPTION_CSIZE  "csize"
#define ALIAS_CSIZE   "c"

#define OPTION_COUNT  "count"
#define ALIAS_COUNT   "n"

#define OPTION_POS2   "pos2"
#define ALIAS_POS2    "2"

static const char * pos_usage[]     = { "where to start reading", NULL };
static const char * qsize_usage[]     = { "size of queue", NULL };
static const char * bsize_usage[]     = { "block-size of queue", NULL };
static const char * csize_usage[]     = { "chunk-size in loop", NULL };
static const char * count_usage[]     = { "how much bytes total", NULL };
static const char * pos2_usage[]     = { "2nd position to read at", NULL };

OptDef QfileTestOptions[] =
{
/*    name             alias        fkt.  usage-txt,  cnt, needs value, required */
    { OPTION_POS,      ALIAS_POS,   NULL, pos_usage,    1,   true,        false },
    { OPTION_QSIZE,    ALIAS_QSIZE, NULL, qsize_usage,  1,   true,        false },
    { OPTION_BSIZE,    ALIAS_BSIZE, NULL, bsize_usage,  1,   true,        false },
    { OPTION_CSIZE,    ALIAS_CSIZE, NULL, csize_usage,  1,   true,        false },
    { OPTION_COUNT,    ALIAS_COUNT, NULL, count_usage,  1,   true,        false },
    { OPTION_POS2,     ALIAS_POS2,  NULL, pos2_usage,   1,   true,        false }
};

const char UsageDefaultName[] = "qfiletest";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [options]\n"
                    "\n", progname);
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

    HelpOptionLine ( ALIAS_POS,   OPTION_POS,   "pos",   pos_usage );
    HelpOptionLine ( ALIAS_QSIZE, OPTION_QSIZE, "qsize", qsize_usage );
    HelpOptionLine ( ALIAS_BSIZE, OPTION_BSIZE, "bsize", bsize_usage );
    HelpOptionLine ( ALIAS_CSIZE, OPTION_CSIZE, "csize", csize_usage );
    HelpOptionLine ( ALIAS_COUNT, OPTION_COUNT, "count", count_usage );
    HelpOptionLine ( ALIAS_POS2,  OPTION_POS2,  "pos2",  pos2_usage );

    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );
    return rc;
}


/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}


static uint64_t get_uint64_option( const Args *my_args,
                                   const char *name,
                                   const uint64_t def )
{
    uint32_t count;
    uint64_t res = def;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( ( rc == 0 )&&( count > 0 ) )
    {
        const char *s;
        rc = ArgsOptionValue( my_args, name, 0,  (const void **)&s );
        if ( rc == 0 ) res = strtoull( s, NULL, 10 );
    }
    return res;
}

static uint32_t get_uint32_option( const Args *my_args,
                                   const char *name,
                                   const uint32_t def )
{
    uint32_t count, res = def;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( ( rc == 0 )&&( count > 0 ) )
    {
        const char *s;
        rc = ArgsOptionValue( my_args, name, 0,  (const void **)&s );
        if ( rc == 0 ) res = strtoul( s, NULL, 10 );
    }
    return res;
}


static rc_t read_loop( const KFile * f, const uint64_t a_pos, const uint64_t count,
                       const size_t chunk_size, uint8_t digest [ 16 ] )
{
    rc_t rc = 0;
    uint64_t pos = a_pos;
    uint64_t n_bytes = 0;
    size_t num_read = 1;
    MD5State md5;

    char * buffer = malloc( chunk_size );
    if ( buffer == NULL )
        return RC( rcExe, rcFile, rcPacking, rcMemory, rcExhausted );

    MD5StateInit ( &md5 );
    while ( rc == 0 && num_read > 0 )
    {
        size_t chunk = chunk_size;

        if ( ( count > 0 ) && ( ( n_bytes + chunk ) > count ) )
        {
            chunk = ( count - n_bytes );
        }

        OUTMSG(( "about to read from pos %lu\n", pos ));
        rc = KFileRead ( f, pos, buffer, chunk, &num_read );
        OUTMSG(( "returned from KFileRead rc = %R, num_read = %zu\n\n", rc, num_read ));
        if ( rc == 0 && num_read > 0 )
        {
            MD5StateAppend ( &md5, buffer, num_read );
            pos += num_read;
            n_bytes += num_read;
            if ( ( count > 0 ) && ( n_bytes >= count ) )
            {
                num_read = 0;
            }
        }
    }
    OUTMSG(( "%lu bytes read total\n", n_bytes ));
    free( buffer );
    MD5StateFinish ( &md5, digest );

    return rc;
}


static void print_digest( uint8_t digest [ 16 ] )
{
    uint8_t i;

    OUTMSG(( "md5-sum: " ));
    for ( i = 0; i < 16; ++i )
        OUTMSG(( "%.02x", digest[ i ] ));
    OUTMSG(( "\n" ));
}


static rc_t perform_qtest( const KFile *f, const char * filename,
                           const uint64_t pos, const uint64_t count,
                           const size_t queue_size, const size_t block_size,
                           const size_t chunk_size )
{
    const KFile * qf;
    rc_t rc = KQueueFileMakeRead ( &qf, pos, f, queue_size, block_size, 0 );
    if ( rc != 0 )
    {
        OUTMSG(( "KQueueFileMakeRead( '%s' ) failed: %R\n", filename, rc ));
    }
    else
    {
        uint8_t digest [ 16 ];
        rc = read_loop( qf, pos, count, chunk_size, digest );
        KFileRelease( qf );
        print_digest( digest );
    }
    return rc;
}


static rc_t perform_test( const KDirectory *dir, const char * filename,
                          const uint64_t pos, const uint64_t pos2,
                          const uint64_t count, const size_t queue_size,
                          const size_t block_size, const size_t chunk_size )
{
    const KFile * f;
    rc_t rc = KDirectoryOpenFileRead ( dir, &f, filename );
    if ( rc != 0 )
    {
        OUTMSG(( "KDirectoryOpenFileRead( '%s' ) failed: %R\n", filename, rc ));
    }
    else
    {
        rc = perform_qtest( f, filename, pos, count, queue_size, block_size, chunk_size );
        if ( rc == 0 && pos2 > 0 )
        {
            rc = perform_qtest( f, filename, pos2, count, queue_size, block_size, chunk_size );
        }
        KFileRelease( f );
    }
    return rc;
}


int main ( int argc, char *argv [] )
{
    Args * args;

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                QfileTestOptions, sizeof ( QfileTestOptions ) / sizeof ( OptDef ) );
    if ( rc != 0 )
    {
        OUTMSG(( "ArgsMakeAndHandle( ... ) failed: %R\n", rc ));
    }
    else
    {
        uint32_t arg_count = 0;
        rc = ArgsParamCount ( args, &arg_count );
        if ( rc != 0 )
        {
            OUTMSG(( "ArgsParamCount( ... ) failed: %R\n", rc ));
        }
        else
        {
            KDirectory *dir = NULL;
            uint64_t position = get_uint64_option( args, OPTION_POS, 0 );
            uint64_t count = get_uint64_option( args, OPTION_COUNT, 0 );
            uint32_t qsize = get_uint32_option( args, OPTION_QSIZE, 1024 );
            uint32_t bsize = get_uint32_option( args, OPTION_BSIZE, 0 );
            uint32_t csize = get_uint32_option( args, OPTION_CSIZE, 1024 );
            uint64_t pos2 = get_uint64_option( args, OPTION_POS2, 0 );

            OUTMSG(( "start-position : %lu\n", position ));
            if ( pos2 > 0 )
            {
                OUTMSG(( "2nd position   : %lu\n", pos2 ));
            }
            if ( count > 0 )
            {
                OUTMSG(( "count          : %lu\n", count ));
            }
            OUTMSG(( "queue-size     : %u\n",  qsize ));
            OUTMSG(( "block-size     : %u\n",  bsize ));
            OUTMSG(( "chunk-size     : %u\n",  csize ));

            rc = KDirectoryNativeDir( &dir );
            if ( rc != 0 )
            {
                OUTMSG(( "KDirectoryNativeDir( ... ) failed: %R\n", rc ));
            }
            else
            {
                uint32_t i;
                for ( i = 0; i < arg_count && rc == 0; ++i )
                {
                    const char * filename;
                    rc = ArgsParamValue ( args, i, (const void **)&filename );
                    if ( rc != 0 )
                    {
                        OUTMSG(( "ArgsParamValue( %d ) failed: %R\n", i, rc ));
                    }
                    else
                    {
                        OUTMSG(( "file to read   : '%s'\n", filename ));
                        rc = perform_test( dir, filename, position, pos2,
                                           count, qsize, bsize, csize );
                    }
                }
                KDirectoryRelease( dir );
            }
        }
        ArgsWhack ( args );
    }

    return (int)rc;
}
