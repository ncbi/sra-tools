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

#include "kget.vers.h"

#include <kapp/main.h>
#include <kapp/args.h>

#include <klib/out.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/cacheteefile.h>

#include <kns/manager.h>
#include <kns/kns-mgr-priv.h>
#include <kns/http.h>

#include <sysalloc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*===========================================================================

    kget is a works like a simple version of wget

 =========================================================================== */

const char UsageDefaultName[] = "kget";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <path> [<path> ...] [options]\n"
                    "\n", progname);
}


#define OPTION_VERB "verb"
#define ALIAS_VERB  "b"
static const char * verb_usage[]       = { "execute verbose", NULL };

#define OPTION_BLOCK "block-size"
#define ALIAS_BLOCK  "s"
static const char * block_usage[]      = { "how many bytes per block", NULL };

#define OPTION_SHOW  "show-size"
#define ALIAS_SHOW   "w"
static const char * show_usage[]       = { "query size of remote file first", NULL };

#define OPTION_CACHE "cache"
#define ALIAS_CACHE  "c"
static const char * cache_usage[]      = { "wrap the remote-file into a KCacheTeeFile", NULL };

#define OPTION_RAND  "random"
#define ALIAS_RAND   "r"
static const char * random_usage[]     = { "request blocks in random order", NULL };

#define OPTION_REP   "repeat"
#define ALIAS_REP    "e"
static const char * repeat_usage[]     = { "request blocks with repeats if in random order", NULL };

#define OPTION_CREPORT "report"
#define ALIAS_CREPORT  "p"
static const char * creport_usage[]    = { "report cache usage", NULL };

#define OPTION_CLUSTER "cluster"
#define ALIAS_CLUSTER  "t"
static const char * cluster_usage[]    = { "used cluster-size in KCacheTeeFile", NULL };

#define OPTION_LOG "log"
static const char * log_usage[]        = { "file to write log into in case of KCacheTeeFile", NULL };

#define OPTION_COMPLETE "complete"
static const char * complete_usage[]   = { "check if 1st parameter is a complete", NULL };

#define OPTION_TRUNC "truncate"
static const char * truncate_usage[]   = { "truncate the file ( 1st parameter ) / remove trailing cache-bitmap", NULL };

#define OPTION_START "start"
static const char * start_usage[]   = { "offset where to read from", NULL };

#define OPTION_COUNT "count"
static const char * count_usage[]   = { "number of bytes to read", NULL };

#define OPTION_PROGRESS "progress"
static const char * progress_usage[]   = { "show progress", NULL };

#define OPTION_RELIABLE "reliable"
static const char * reliable_usage[]   = { "use reliable version of http-file", NULL };

OptDef MyOptions[] =
{
/*    name            alias         fkt   usage-txt,      cnt, needs value, required */
    { OPTION_VERB,    ALIAS_VERB,    NULL, verb_usage,     1,   false,       false },
    { OPTION_BLOCK,   ALIAS_BLOCK,   NULL, block_usage,    1,   true,        false },
    { OPTION_SHOW,    ALIAS_SHOW,    NULL, show_usage,     1,   false,       false },
    { OPTION_CACHE,   ALIAS_CACHE,   NULL, cache_usage,    1,   true,        false },
    { OPTION_RAND,    ALIAS_RAND,    NULL, random_usage,   1,   false,       false },
    { OPTION_REP,     ALIAS_REP,     NULL, repeat_usage,   1,   false,       false },
    { OPTION_CREPORT, ALIAS_CREPORT, NULL, creport_usage,  1,   false,       false },
    { OPTION_CLUSTER, ALIAS_CLUSTER, NULL, cluster_usage,  1,   true,        false },
    { OPTION_LOG,     NULL,          NULL, log_usage,      1,   true,        false },
    { OPTION_COMPLETE,NULL,          NULL, complete_usage, 1,   false,       false },
    { OPTION_TRUNC,   NULL,          NULL, truncate_usage, 1,   false,       false },
    { OPTION_START,   NULL,          NULL, start_usage,    1,   true,        false },
    { OPTION_COUNT,   NULL,          NULL, count_usage,    1,   true,        false },
    { OPTION_PROGRESS,NULL,          NULL, progress_usage, 1,   false,       false },
    { OPTION_RELIABLE,NULL,          NULL, reliable_usage, 1,   false,       false }
};

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    uint32_t i;
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
    for ( i = 0; i < sizeof ( MyOptions ) / sizeof ( MyOptions[ 0 ] ); ++i )
        HelpOptionLine ( MyOptions[i].aliases, MyOptions[i].name, NULL, MyOptions[i].help );

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
    return KGET_VERS;
}


typedef struct fetch_ctx
{
    const char *url;
    const char *destination;
    const char *cache_file;
    const char *log_file;
    size_t blocksize;
    size_t cluster;
    size_t start, count;
    bool verbose;
    bool show_filesize;
    bool random;
    bool with_repeats;
    bool show_curl_version;
    bool report_cache;
    bool check_cache_complete;
    bool truncate_cache;
    bool local_read_only;
    bool show_progress;
	bool reliable;
} fetch_ctx;


static rc_t block_loop_in_order( const KFile *src, KFile *dst, char * buffer, 
                                 uint64_t *bytes_copied, fetch_ctx * ctx )
{
    rc_t rc = 0;
    uint64_t pos = 0;
    size_t num_read = 1;
    KOutMsg( "copy-mode : linear read/write\n" );
    while ( rc == 0 && num_read > 0 )
    {
        rc = KFileReadAll ( src, pos, buffer, ctx->blocksize, &num_read );
        if ( rc == 0 && num_read > 0 )
        {
            size_t num_writ;
            rc = KFileWriteAll ( dst, pos, buffer, num_read, &num_writ );
            pos += num_read;
            if ( ctx->show_progress )
                KOutMsg( "." );
        }
    }
    *bytes_copied = pos;
    if ( ctx->show_progress )
        KOutMsg( "\n" );
    return rc;
}


static rc_t block_loop_random( const KFile *src, KFile *dst, char * buffer,
                               uint64_t *bytes_copied, fetch_ctx * ctx )
{
    uint64_t src_size;
    rc_t rc = KFileSize ( src, &src_size );
    KOutMsg( "copy-mode : random blocks\n" );
    *bytes_copied = 0;
    if ( rc == 0 )
    {
        rc = KFileSetSize ( dst, src_size );
        if ( rc == 0 )
        {
            uint32_t block_count = ( src_size / ctx->blocksize ) + 1;
            char * block_vector = malloc( block_count );
            if ( block_vector != NULL )
            {
                uint32_t loop_count = 0;
                while( ( rc == 0 ) &&
                       ( *bytes_copied < src_size ) &&
                       ( loop_count < ( 5 * block_count ) ) )
                {
                    uint32_t random_block = ( rand() % block_count );
                    if ( ctx->with_repeats || block_vector[ random_block ] == 0 )
                    {
                        size_t num_read;
                        uint64_t pos = ctx->blocksize;
                        pos *= random_block;

                        KOutMsg( "random_block = %u at pos %lu\n", random_block, pos );

                        rc = KFileReadAll ( src, pos, buffer, ctx->blocksize, &num_read );
                        if ( rc == 0 )
                        {
                            size_t num_writ;
                            rc = KFileWriteAll ( dst, pos, buffer, num_read, &num_writ );

                            if ( block_vector[ random_block ] == 0 )
                            {
                                block_vector[ random_block ] = 1;
                                *bytes_copied += num_read;
                            }
                            KOutMsg( "%u bytes copied in block %u ( %lu of %lu )\n\n", 
                                     num_writ, random_block, *bytes_copied, src_size );
                        }
                    }
                    ++loop_count;
                }
                free( block_vector );
            }
        }
    }
    return rc;
}


static rc_t copy_file( const KFile * src, KFile * dst, fetch_ctx * ctx )
{
    rc_t rc = 0;
    size_t buffer_size = ( ctx->count == 0 ? ctx->blocksize : ctx->count );
    char * buffer = malloc( buffer_size );
    if ( buffer == NULL )
    {
        rc = RC( rcExe, rcFile, rcPacking, rcMemory, rcExhausted );
        KOutMsg( "cant make buffer of size %u\n", buffer_size );
    }
    else
    {
        uint64_t bytes_copied = 0;
        if ( ctx->count == 0 )
        {
            if ( ctx->random )
            {
                rc = block_loop_random( src, dst, buffer, &bytes_copied, ctx );
            }
            else
			{
                rc = block_loop_in_order( src, dst, buffer, &bytes_copied, ctx );
			}
        }
        else
        {
            size_t num_read;
            rc = KFileReadAll ( src, ctx->start, buffer, ctx->count, &num_read );
			if ( rc != 0 )
			{
				(void)LOGERR( klogInt, rc, "KFileReadAll() failed" );				
			}
            else
            {
                size_t num_writ;
                rc = KFileWriteAll ( dst, ctx->start, buffer, num_read, &num_writ );
                if ( rc == 0 )
                    bytes_copied = num_writ;
            }
        }
        KOutMsg( "%lu bytes copied\n", bytes_copied );
        free( buffer );
    }
    return rc;
}


static rc_t fetch_cached( KDirectory *dir, const KFile *src, KFile *dst, fetch_ctx *ctx )
{
    rc_t rc = 0;
    KFile *log_file = NULL;    /* into this file the log is written */

    if ( ctx->log_file != NULL )
        rc = KDirectoryCreateFile ( dir, &log_file, false, 0664, kcmOpen, ctx->log_file );

    if ( rc == 0 )
    {
        const KFile *tee; /* this is the file that forks persistent_content with remote */

        KOutMsg( "persistent cache created\n" );

        rc = KDirectoryMakeCacheTee ( dir, &tee, src, log_file, 0, ctx->cluster, 
                                      ctx->report_cache, ctx->cache_file );
        if ( rc == 0 )
        {
            KOutMsg( "cache tee created\n" );
            rc = copy_file( tee, dst, ctx );
            KFileRelease( tee );
        }
    }
    KFileRelease( log_file );

    return rc;
}


static void extract_name( char ** dst, const char * url )
{
    char * last_slash = string_rchr ( url, string_size( url ), '/' );
    if ( last_slash == NULL )
        *dst = string_dup_measure( "out.bin", NULL );
    else
        *dst = string_dup_measure( last_slash + 1, NULL );
}


static rc_t fetch_from( KDirectory *dir, fetch_ctx *ctx, char * outfile,
						const KFile * src )
{
	uint64_t file_size;
    rc_t rc = KFileSize( src, &file_size );
	if ( rc != 0 )
	{
		KOutMsg( "cannot disover src-size >%R<\n", rc );
	}
	else
	{
		KFile *dst;
		KOutMsg( "src-size = %lu\n", file_size );
		rc = KDirectoryCreateFile ( dir, &dst, false, 0664, kcmInit, outfile );
		if ( rc == 0 )
		{
			KOutMsg( "dst >%s< created\n", outfile );
			if ( rc == 0 )
			{
				if ( ctx->cache_file != NULL )
					rc = fetch_cached( dir, src, dst, ctx );
				else
					rc = copy_file( src, dst, ctx );
			}
			KFileRelease( dst );
		}
	}
	return rc;
}


static rc_t make_remote_file( struct KNSManager * kns_mgr, const KFile ** src, fetch_ctx * ctx )
{
	rc_t rc;
	
	KNSManagerSetVerbose( kns_mgr, ctx->verbose );
	if ( ctx->reliable )
		rc = KNSManagerMakeReliableHttpFile( kns_mgr, src, NULL, 0x01010000, ctx->url );
	else
		rc = KNSManagerMakeHttpFile ( kns_mgr, src, NULL, 0x01010000, ctx->url );
	
	if ( rc != 0 )
	{
		if ( ctx->reliable )
			(void)LOGERR( klogInt, rc, "KNSManagerMakeReliableHttpFile() failed" );
		else
			(void)LOGERR( klogInt, rc, "KNSManagerMakeHttpFile() failed" );
	}
	return rc;
}


static rc_t fetch( KDirectory *dir, fetch_ctx *ctx )
{
    rc_t rc = 0;
    char * outfile;
    const KFile * remote;
	struct KNSManager * kns_mgr;
	
    if ( ctx->destination == NULL )
        extract_name( &outfile, ctx->url );
    else
        outfile = string_dup_measure( ctx->destination, NULL );

    KOutMsg( "fetching: >%s<\n", ctx->url );
    KOutMsg( "into    : >%s<\n", outfile );
    if ( ctx->count > 0 )
        KOutMsg( "range   : %u.%u\n", ctx->start, ctx->count );

	rc = KNSManagerMake ( &kns_mgr );
	if ( rc != 0 )
		(void)LOGERR( klogInt, rc, "KNSManagerMake() failed" );
	else
	{
		rc = make_remote_file( kns_mgr, &remote, ctx );
		if ( rc == 0 )
		{
			rc = fetch_from( dir, ctx, outfile, remote );
			KFileRelease( remote );
		}
		KNSManagerRelease( kns_mgr );
	}

	free( outfile );
    return rc;
}

static rc_t show_size( KDirectory *dir, fetch_ctx *ctx )
{
    rc_t rc = 0;
    const KFile * remote;
	struct KNSManager * kns_mgr;
	
    KOutMsg( "source: >%s<\n", ctx->url );
	rc = KNSManagerMake ( &kns_mgr );
	if ( rc != 0 )
		(void)LOGERR( klogInt, rc, "KNSManagerMake() failed" );
	else
	{
		rc = make_remote_file( kns_mgr, &remote, ctx );
		if ( rc == 0 )
		{
			uint64_t file_size;
			rc = KFileSize( remote, &file_size );
			KOutMsg( "file-size = %u\n", file_size );
			KFileRelease( remote );
		}
		KNSManagerRelease( kns_mgr );
	}
    return rc;
}


static rc_t check_cache_complete( KDirectory *dir, fetch_ctx *ctx )
{
    rc_t rc = 0;
    const KFile *f;

    KOutMsg( "checking if this cache file >%s< is complete\n", ctx->url );

    rc = KDirectoryOpenFileRead( dir, &f, "%s", ctx->url );
    if ( rc == 0 )
    {
        bool is_complete;
        rc = IsCacheFileComplete( f, &is_complete, false );
        if ( rc != 0 )
            KOutMsg( "error performing IsCacheFileComplete() %R\n", rc );
        else
        {
            if ( is_complete )
                KOutMsg( "the file is complete\n" );
            else
            {
                float percent = 0;
                rc = GetCacheCompleteness( f, &percent );
                if ( rc == 0 )
                    KOutMsg( "the file is %f%% complete\n", percent );
            }
        }
        KFileRelease( f );
    }
    return rc;
}


static rc_t truncate_cache( KDirectory *dir, fetch_ctx *ctx )
{
    rc_t rc = 0;
    KFile *f;

    KOutMsg( "truncating this cache file >%s< (from it's trailing bitmap)\n", ctx->url );

    rc = KDirectoryOpenFileWrite( dir, &f, true, "%s", ctx->url );
    if ( rc == 0 )
    {
        rc = TruncateCacheFile( f );
        if ( rc == 0 )
            KOutMsg( "the file was truncated\n" );
        else
            KOutMsg( "the file was not truncated: %R\n", rc );
        KFileRelease( f );
    }
    return rc;
}


rc_t get_bool( Args * args, const char *option, bool *value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option, &count );
    *value = ( rc == 0 && count > 0 );
    return rc;
}


rc_t get_str( Args * args, const char *option, const char ** value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option, &count );
    *value = NULL;
    if ( rc == 0 && count > 0 )
        rc = ArgsOptionValue( args, option, 0, value );
    return rc;
}


rc_t get_size_t( Args * args, const char *option, size_t *value, size_t dflt )
{
    const char * s;
    rc_t rc = get_str( args, option, &s );
    *value = dflt;
    if ( rc == 0 && s != NULL )
        *value = atoi( s );
    else
        *value = dflt;
    return rc;
}


rc_t get_fetch_ctx( Args * args, fetch_ctx * ctx )
{
    rc_t rc = 0;
    uint32_t count;

    ctx->url = NULL;
    ctx->verbose = false;
    rc = ArgsParamCount( args, &count );
    if ( rc == 0 && count > 0 )
    {
        rc = ArgsParamValue( args, 0, &ctx->url );
        if ( rc == 0 )
        {
            if ( count > 1 )
                rc = ArgsParamValue( args, 1, &ctx->destination );
            else
                ctx->destination = NULL;
        }
    }

    if ( rc == 0 ) rc = get_bool( args, OPTION_VERB, &ctx->verbose );
    if ( rc == 0 ) rc = get_bool( args, OPTION_SHOW, &ctx->show_filesize );
    if ( rc == 0 ) rc = get_str( args, OPTION_CACHE, &ctx->cache_file );
    if ( rc == 0 ) rc = get_bool( args, OPTION_RAND, &ctx->random );
    if ( rc == 0 ) rc = get_bool( args, OPTION_REP, &ctx->with_repeats );
    if ( rc == 0 ) rc = get_bool( args, OPTION_CREPORT, &ctx->report_cache );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_BLOCK, &ctx->blocksize, ( 32 * 1024 ) );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_CLUSTER, &ctx->cluster, 1 );
    if ( rc == 0 ) rc = get_str( args, OPTION_LOG, &ctx->log_file );
    if ( rc == 0 ) rc = get_bool( args, OPTION_COMPLETE, &ctx->check_cache_complete );
    if ( rc == 0 ) rc = get_bool( args, OPTION_TRUNC, &ctx->truncate_cache );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_START, &ctx->start, 0 );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_COUNT, &ctx->count, 0 );
    if ( rc == 0 ) rc = get_bool( args, OPTION_PROGRESS, &ctx->show_progress );
    if ( rc == 0 ) rc = get_bool( args, OPTION_RELIABLE, &ctx->reliable );
	
    return rc;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc;

    rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                MyOptions, sizeof ( MyOptions ) / sizeof ( OptDef ) );
    if ( rc == 0 )
    {
        fetch_ctx ctx;

        rc = get_fetch_ctx( args, &ctx );
        if ( rc == 0 )
        {
            if ( ctx.url == NULL )
				KOutMsg( "URL is missing!\n" );	
			else
            {
                KDirectory *dir;
                rc = KDirectoryNativeDir ( &dir );
                if ( rc == 0 )
                {
                    if ( ctx.check_cache_complete )
                        rc = check_cache_complete( dir, &ctx );
                    else if ( ctx.truncate_cache )
                        rc = truncate_cache( dir, &ctx );
                    else if ( ctx.show_filesize )
                        rc = show_size( dir, &ctx );
                    else
                        rc = fetch( dir, &ctx );

                    KDirectoryRelease ( dir );
                }
            }
        }
        ArgsWhack ( args );
    }
    else
        KOutMsg( "ArgsMakeAndHandle() failed %R\n", rc );

    return rc;
}
