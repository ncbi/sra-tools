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
#include <kapp/args.h>

#include <klib/out.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/time.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/buffile.h>
#include <kfs/cacheteefile.h>
//#include <kfs/big_block_reader.h>

#include <kns/manager.h>
#include <kns/kns-mgr-priv.h>
#include <kns/http.h>
#include <kns/stream.h>

#include <kproc/timeout.h>

#include <os-native.h>
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
static const char * verb_usage[]        = { "execute verbose", NULL };

#define OPTION_BLOCK "block-size"
#define ALIAS_BLOCK  "s"
static const char * block_usage[]       = { "how many bytes per block", NULL };

#define OPTION_SHOW  "show-size"
#define ALIAS_SHOW   "w"
static const char * show_usage[]        = { "query size of remote file first", NULL };

#define OPTION_CACHE "cache"
#define ALIAS_CACHE  "c"
static const char * cache_usage[]       = { "wrap the remote-file into a KCacheTeeFile", NULL };

#define OPTION_CACHE_BLK "cache-block"
static const char * cache_blk_usage[]   = { "blocksize inside a KCacheTeeFile", NULL };

#define OPTION_PROXY "proxy"
static const char * proxy_usage[]       = { "use a proxy to download remote file", NULL };

#define OPTION_RAND  "random"
#define ALIAS_RAND   "r"
static const char * random_usage[]      = { "request blocks in random order", NULL };

#define OPTION_REP   "repeat"
#define ALIAS_REP    "e"
static const char * repeat_usage[]      = { "request blocks with repeats if in random order", NULL };

#define OPTION_CREPORT "report"
#define ALIAS_CREPORT  "p"
static const char * creport_usage[]     = { "report cache usage", NULL };

#define OPTION_COMPLETE "complete"
static const char * complete_usage[]    = { "check if 1st parameter is a complete", NULL };

#define OPTION_TRUNC "truncate"
static const char * truncate_usage[]    = { "truncate the file ( 1st parameter ) / remove trailing cache-bitmap", NULL };

#define OPTION_START "start"
static const char * start_usage[]       = { "offset where to read from", NULL };

#define OPTION_COUNT "count"
static const char * count_usage[]       = { "number of bytes to read", NULL };

#define OPTION_PROGRESS "progress"
static const char * progress_usage[]    = { "show progress", NULL };

#define OPTION_RELIABLE "reliable"
static const char * reliable_usage[]    = { "use reliable version of http-file", NULL };

#define OPTION_BUFFER "buffer"
#define ALIAS_BUFFER  "u"
static const char * buffer_usage[]      = { "wrap remote file into KBufFile with this buffer-size", NULL };

#define OPTION_SLEEP "sleep"
#define ALIAS_SLEEP  "i"
static const char * sleep_usage[]       = { "sleep inbetween requests by this amount of ms", NULL };

#define OPTION_TIMEOUT "timeout"
#define ALIAS_TIMEOUT "m"
static const char * timeout_usage[]     = { "use timed read with tis amount of ms as timeout", NULL };

#define OPTION_CCOMPL "cache-complete"
#define ALIAS_CCOMPL "a"
static const char * ccompl_usage[]      = { "check completeness on open cacheteefile", NULL };

#define OPTION_FULL "full"
#define ALIAS_FULL "f"
static const char * full_usage[]        = { "download via one http-request, not partial requests in a loop", NULL };

OptDef MyOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_VERB,      ALIAS_VERB,     NULL, verb_usage,       1,  false,       false },
    { OPTION_BLOCK,     ALIAS_BLOCK,    NULL, block_usage,      1,  true,        false },
    { OPTION_SHOW,      ALIAS_SHOW,     NULL, show_usage,       1,  false,       false },
    { OPTION_CACHE,     ALIAS_CACHE,    NULL, cache_usage,      1,  true,        false },
    { OPTION_CACHE_BLK, NULL,           NULL, cache_blk_usage,  1,  true,        false },
    { OPTION_PROXY,     NULL,           NULL, proxy_usage,      1,  true,        false },
    { OPTION_RAND,      ALIAS_RAND,     NULL, random_usage,     1,  false,       false },
    { OPTION_REP,       ALIAS_REP,      NULL, repeat_usage,     1,  false,       false },
    { OPTION_CREPORT,   ALIAS_CREPORT,  NULL, creport_usage,    1,  false,       false },
    { OPTION_BUFFER,    ALIAS_BUFFER,   NULL, buffer_usage,     1,  true,        false },
    { OPTION_SLEEP,     ALIAS_SLEEP,    NULL, sleep_usage,      1,  true,        false },    
    { OPTION_TIMEOUT,   ALIAS_TIMEOUT,  NULL, timeout_usage,    1,  true,        false },
    { OPTION_COMPLETE,  NULL,           NULL, complete_usage,   1,  false,       false },
    { OPTION_CCOMPL,    ALIAS_CCOMPL,   NULL, ccompl_usage,     1,  false,       false },
    { OPTION_TRUNC,     NULL,           NULL, truncate_usage,   1,  false,       false },
    { OPTION_START,     NULL,           NULL, start_usage,      1,  true,        false },
    { OPTION_COUNT,     NULL,           NULL, count_usage,      1,  true,        false },
    { OPTION_PROGRESS,  NULL,           NULL, progress_usage,   1,  false,       false },
    { OPTION_RELIABLE,  NULL,           NULL, reliable_usage,   1,  false,       false },
    { OPTION_FULL,      ALIAS_FULL,     NULL, full_usage,       1,  false,       false }
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

typedef struct fetch_ctx
{
    const char *url;
    const char *destination;
    const char *cache_file;
    const char * proxy;
    size_t blocksize;
    size_t start, count;
    size_t buffer_size;
    size_t sleep_time;
    size_t timeout_time;
    size_t cache_blk;
    bool verbose;
    bool show_filesize;
    bool random;
    bool with_repeats;
    bool show_curl_version;
    bool report_cache;
    bool check_cache_complete;
    bool check_completeness;    
    bool truncate_cache;
    bool local_read_only;
    bool show_progress;
    bool reliable;
    bool full_download;
} fetch_ctx;


static rc_t src_2_dst( const KFile *src, KFile *dst, char * buffer,
                       uint64_t pos, size_t * num_read, fetch_ctx * ctx )
{
    rc_t rc;
    size_t n_transfer = ( ctx->count == 0 ? ctx->blocksize : ctx->count );
    
    if ( ctx->timeout_time == 0 )
        rc = KFileReadAll ( src, pos, buffer, n_transfer, num_read );
    else
    {
        timeout_t tm;
        rc = TimeoutInit ( &tm, ctx->timeout_time );
        if ( rc == 0 )
            rc = KFileTimedReadAll ( src, pos, buffer, n_transfer, num_read, &tm );
    }
    if ( rc == 0 && *num_read > 0 )
    {
        size_t num_writ;
        rc = KFileWriteAll ( dst, pos, buffer, *num_read, &num_writ );
    }
    return rc;
}


static rc_t block_loop_in_order( const KFile *src, KFile *dst, char * buffer, 
                                 uint64_t * bytes_copied, fetch_ctx * ctx )
{
    rc_t rc = 0;
    uint64_t pos = 0;
    uint32_t blocks = 0;
    size_t num_read = 1;
    KOutMsg( "copy-mode : linear read/write\n" );
    while ( rc == 0 && num_read > 0 )
    {
        rc = src_2_dst( src, dst, buffer, pos, &num_read, ctx );
        if ( rc == 0 ) pos += num_read;
        if ( ctx->show_progress && ( ( blocks & 0x0F ) == 0 ) ) KOutMsg( "." );
        blocks++;
        if ( ctx->sleep_time > 0 ) KSleepMs( ctx->sleep_time );
    }
    *bytes_copied = pos;
    if ( ctx->show_progress ) KOutMsg( "\n" );
    KOutMsg( "%d blocks a %d bytes\n", blocks, ctx->blocksize );
    return rc;
}



static uint32_t randr( uint32_t min, uint32_t max )
{
       double scaled = ( ( double )rand() / RAND_MAX );
       return ( ( max - min + 1 ) * scaled ) + min;
}


static rc_t block_loop_random( const KFile *src, KFile *dst, char * buffer,
                               uint64_t *bytes_copied, fetch_ctx * ctx )
{
    uint64_t src_size;
    rc_t rc = KFileSize ( src, &src_size );
    KOutMsg( "copy-mode : random blocks\n" );
    if ( rc == 0 )
    {
        rc = KFileSetSize ( dst, src_size );
        if ( rc == 0 )
        {
            uint32_t block_count = ( src_size / ctx->blocksize ) + 1;
            uint32_t * block_vector = malloc( block_count * ( sizeof * block_vector ) );
            if ( block_vector != NULL )
            {
                uint32_t loop;
                
                /* fill the block_vector with ascending numbers */
                for ( loop = 0; loop < block_count; loop++ )
                    block_vector[ loop ] = loop;
                
                /* randomize them */
                for ( loop = 0; loop < block_count; loop++ )
                {
                    uint32_t src_idx = randr( 0, block_count - 1 );
                    uint32_t dst_idx = randr( 0, block_count - 1 );
                    /* swap it... */
                    uint32_t tmp = block_vector[ dst_idx ];
                    block_vector[ dst_idx ] = block_vector[ src_idx ];
                    block_vector[ src_idx ] = tmp;
                }

                for ( loop = 0; rc == 0 && loop < block_count; loop++ )
                {
                    size_t num_read;
                    uint64_t pos = ctx->blocksize;
                    pos *= block_vector[ loop ];
                    rc = src_2_dst( src, dst, buffer, pos, &num_read, ctx );
                    if ( rc == 0 ) *bytes_copied += num_read;
                    if ( ctx->show_progress && ( ( loop & 0x0F ) == 0 ) ) KOutMsg( "." );
                    if ( ctx->sleep_time > 0 ) KSleepMs( ctx->sleep_time );

                }
                free( block_vector );
                if ( ctx->show_progress ) KOutMsg( "\n" );
                KOutMsg( "%d blocks a %d bytes\n", loop, ctx->blocksize );
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
                rc = block_loop_random( src, dst, buffer, &bytes_copied, ctx );
            else
                rc = block_loop_in_order( src, dst, buffer, &bytes_copied, ctx );
        }
        else
        {
            size_t num_read;
            rc = src_2_dst( src, dst, buffer, ctx->start, &num_read, ctx );
            if ( rc == 0 ) bytes_copied = num_read;
        }
        KOutMsg( "%lu bytes copied\n", bytes_copied );
        free( buffer );
    }
    return rc;
}


#define CACHE_TEE_DEFAULT_BLOCKSIZE ( 32 * 1024 * 4 )

static rc_t fetch_cached( KDirectory *dir, const KFile *src, KFile *dst, fetch_ctx *ctx )
{
    size_t bs = ctx->cache_blk == 0 ? CACHE_TEE_DEFAULT_BLOCKSIZE : ctx->cache_blk;
    rc_t rc = KOutMsg( "persistent cache created : '%s' (blk-size: %d)\n", ctx->cache_file, bs );
    if ( rc == 0 )
    {
        const KFile *tee; /* this is the file that forks persistent_content with remote */
        rc = KDirectoryMakeCacheTee ( dir,                  /* the KDirectory for the the sparse-file */
                                      &tee,                 /* the newly created cache-tee-file */
                                      src,                  /* the file that we are wrapping ( usually the remote http-file ) */
                                      ctx->cache_blk,       /* how big one block in the cache-tee-file will be */
                                      ctx->cache_file );    /* the sparse-file we use write to */
        if ( rc == 0 )
        {
            KOutMsg( "cache tee created\n" );
            rc = copy_file( tee, dst, ctx );
            KFileRelease( tee );
        }
    }
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
        KOutMsg( "cannot disover src-size >%R<\n", rc );
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
        rc = KNSManagerMakeHttpFile( kns_mgr, src, NULL, 0x01010000, ctx->url );
    
    if ( rc != 0 )
    {
        if ( ctx->reliable )
            (void)LOGERR( klogInt, rc, "KNSManagerMakeReliableHttpFile() failed" );
        else
            (void)LOGERR( klogInt, rc, "KNSManagerMakeHttpFile() failed" );
    }
    else
    {
        if ( ctx->buffer_size > 0 )
        {
            const KFile * temp_file;
            rc = KBufFileMakeRead ( & temp_file, *src, ctx->buffer_size );
            if ( rc == 0 )
            {
                KOutMsg( "remote-file wrapped in new big-block-reader of size %d\n", ctx->buffer_size );
                KFileRelease ( *src );
                *src = temp_file;
            }
        }
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
        if ( ctx->proxy != NULL )
        {
            rc = KNSManagerSetHTTPProxyPath( kns_mgr, "%s", ctx->proxy );
            if ( rc != 0 )
                (void)LOGERR( klogInt, rc, "KNSManagerSetHTTPProxyPath() failed" );    
        }
        if ( rc == 0 )
        {
            rc = make_remote_file( kns_mgr, &remote, ctx );
            if ( rc == 0 )
            {
                rc = fetch_from( dir, ctx, outfile, remote );
                KFileRelease( remote );
            }
        }
        KNSManagerRelease( kns_mgr );
    }

    free( outfile );
    return rc;
}


/* -------------------------------------------------------------------------------------------------------------------- */


static rc_t show_size( KDirectory *dir, fetch_ctx *ctx )
{
    rc_t rc = KOutMsg( "source: >%s<\n", ctx->url );
    if ( rc == 0 )
    {
        struct KNSManager * kns_mgr;
        rc = KNSManagerMake ( &kns_mgr );
        if ( rc != 0 )
            (void)LOGERR( klogInt, rc, "KNSManagerMake() failed" );
        else
        {
            const KFile * remote;    
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
    }
    return rc;
}


/* -------------------------------------------------------------------------------------------------------------------- */


/* check cache completeness on raw file in the filesystem */
static rc_t check_cache_complete( KDirectory *dir, fetch_ctx *ctx )
{
    rc_t rc = KOutMsg( "checking if this cache file >%s< is complete\n", ctx->url );
    if ( rc == 0 )
    {
        const KFile *f;
        rc = KDirectoryOpenFileRead( dir, &f, "%s", ctx->url );
        if ( rc == 0 )
        {
            bool is_complete;
            rc = IsCacheFileComplete( f, &is_complete );
            if ( rc != 0 )
                KOutMsg( "error performing IsCacheFileComplete() %R\n", rc );
            else
            {
                if ( is_complete )
                    KOutMsg( "the file is complete\n" );
                else
                {
                    float percent = 0;
                    uint64_t bytes_cached;
                    rc = GetCacheCompleteness( f, &percent, &bytes_cached );
                    if ( rc == 0 )
                        KOutMsg( "the file is %f%% complete ( %lu bytes are cached )\n", percent, bytes_cached );
                }
            }
            KFileRelease( f );
        }
    }
    return rc;
}


/* -------------------------------------------------------------------------------------------------------------------- */


static rc_t fetch_loop( const KFile * src, fetch_ctx *ctx )
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
        uint64_t pos = 0;
        size_t num_read = 0;
        do
        {
            rc = KFileReadAll( src, pos, buffer, buffer_size, &num_read );
            if ( rc == 0 )
                pos += num_read;
        } while ( rc == 0 && num_read > 0 );
        KOutMsg( "%lu bytes copied\n", pos );
        free( buffer );
    }
    return rc;
}


/* check cache completeness on a open cacheteefile */
static rc_t check_cache_completeness( KDirectory *dir, fetch_ctx *ctx )
{
    rc_t rc = KOutMsg( "check if IsCacheTeeComplete() works as intended\n" );
    if ( rc == 0 )
    {
        struct KNSManager * kns_mgr;    
        rc = KNSManagerMake ( &kns_mgr );
        if ( rc != 0 )
            (void)LOGERR( klogInt, rc, "KNSManagerMake() failed" );
        else
        {
            const KFile * remote;    
            rc = make_remote_file( kns_mgr, &remote, ctx );
            if ( rc == 0 )
            {
                const KFile *tee; /* this is the file that forks persistent_content with remote */
                rc = KDirectoryMakeCacheTee ( dir,                  /* the KDirectory for the the sparse-file */
                                              &tee,                 /* the newly created cache-tee-file */
                                              remote,               /* the file that we are wrapping ( usually the remote http-file ) */
                                              ctx->cache_blk,       /* how big one block in the cache-tee-file will be */
                                              ctx->cache_file );    /* the sparse-file we use write to */
                if ( rc == 0 )
                    rc = fetch_loop( tee, ctx );
                if ( rc == 0 )
                {
                
                    bool complete = false;
                    rc = IsCacheTeeComplete( tee, &complete );
                    KOutMsg( "IsCacheTeeComplete() -> %R, complete = %s\n", rc, complete ? "YES" : "NO" );
                    KFileRelease( tee );
                }
                KFileRelease( remote );
            }
            KNSManagerRelease( kns_mgr );
        }
    }
    return rc;
}


/* -------------------------------------------------------------------------------------------------------------------- */

/* this is 'borrowed' from libs/kns/http-priv.h :
    - this is a private header inside the source-directory
    - without it KNSManagerMakeClientHttp( ... ) a public function cannot be used
        ( or the user writes it's own URL-parsing )
*/

typedef enum 
{
    st_NONE,
    st_HTTP,
    st_S3
} SchemeType;

typedef struct URLBlock URLBlock;
struct URLBlock
{
    String scheme;
    String host;
    String path; /* Path includes any parameter portion */
    String query;
    String fragment;

    uint32_t port;

    SchemeType scheme_type;
};
extern void URLBlockInit ( URLBlock *self );
extern rc_t ParseUrl ( URLBlock * b, const char * url, size_t url_size );


/* check cache completeness on a open cacheteefile */
static rc_t full_download( KDirectory *dir, fetch_ctx *ctx )
{
    rc_t rc = KOutMsg( "make full download without partial access\n" );
    if ( rc == 0 )
    {
        struct KNSManager * kns_mgr;    
        rc = KNSManagerMake ( &kns_mgr );
        if ( rc != 0 )
            (void)LOGERR( klogInt, rc, "KNSManagerMake() failed" );
        else
        {
            struct URLBlock url;
            URLBlockInit( &url );
            rc = ParseUrl( &url, ctx->url, string_size( ctx->url ) );
            if ( rc == 0 )
            {
                KClientHttp * http;
                rc = KNSManagerMakeClientHttp( kns_mgr, &http, NULL, 0x01010000, &url.host, url.port );
                if ( rc == 0 )
                {
                    KClientHttpRequest * req;
                    KOutMsg( "connection open!\n" );
                    rc = KClientHttpMakeRequest( http, &req, ctx->url );
                    if ( rc == 0 )
                    {
                        KClientHttpResult *rslt;
                        
                        KOutMsg( "request made!\n" );
                        
                        KClientHttpRequestConnection( req, true );
                        KClientHttpRequestSetNoCache( req );
                        
                        rc = KClientHttpRequestGET( req, &rslt ); 
                        if ( rc == 0 )
                        {
                            uint32_t result_code;
                            size_t msg_size;
                            char buffer[ 4096 * 32 ]; /* 128k */
                            
                            KOutMsg( "reply received!\n" );
                            rc = KClientHttpResultStatus( rslt, &result_code, buffer, sizeof buffer, &msg_size );
                            if ( rc == 0 )
                            {
                                struct KStream  *content;
                                KOutMsg( "result-code = %d\n", result_code );
                                if ( result_code == 200 )
                                {
                                    rc = KClientHttpResultGetInputStream( rslt, &content );
                                    if ( rc == 0 )
                                    {
                                        KFile *dst;
                                        char * outfile;
                                        
                                        if ( ctx->destination == NULL )
                                            extract_name( &outfile, ctx->url );
                                        else
                                            string_dup_measure( ctx->destination, NULL );

                                        rc = KDirectoryCreateFile ( dir, &dst, false, 0664, kcmInit, outfile );
                                        if ( rc == 0 )
                                        {
                                            KOutMsg( "dst >%s< created\n", outfile );
                                            if ( rc == 0 )
                                            {
                                                uint64_t pos = 0;
                                                size_t num_read;
                                                struct timeout_t timeout;

                                                TimeoutInit( &timeout, 2000 );                                                
                                                do
                                                {
                                                    rc = KStreamTimedRead( content, buffer, sizeof buffer, &num_read, &timeout );
                                                    if ( rc == 0 )
                                                    {
                                                        size_t num_writ;
                                                        rc = KFileWriteAll( dst, pos, buffer, num_read, &num_writ );
                                                        pos += num_read;
                                                    }
                                                } while ( rc == 0 && num_read > 0 );

                                                KOutMsg( "%d bytes read!\n", pos );
                                            }
                                            KFileRelease( dst );
                                        }
                                        free( outfile );
                                        KStreamRelease( content );
                                    }
                                }
                            }
                            KClientHttpResultRelease( rslt );
                        }
                        KClientHttpRequestRelease( req );
                    }
                    KClientHttpRelease ( http );
                }
            }
            KNSManagerRelease( kns_mgr );
        }
    }
    return rc;
}


/* -------------------------------------------------------------------------------------------------------------------- */


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


/* -------------------------------------------------------------------------------------------------------------------- */


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
        rc = ArgsOptionValue( args, option, 0, (const void **)value );
    return rc;
}


rc_t get_size_t( Args * args, const char *option, size_t *value, size_t dflt )
{
    const char * s;
    rc_t rc = get_str( args, option, &s );
    *value = dflt;
    if ( rc == 0 && s != NULL )
    {
        size_t l = string_size( s );
        if ( l == 0 )
            *value = dflt;
        else
        {
            size_t multipl = 1;
            switch( s[ l - 1 ] )
            {
                case 'k' :
                case 'K' : multipl = 1024; break;
                case 'm' :
                case 'M' : multipl = 1024 * 1024; break;
                case 'g' :
                case 'G' : multipl = 1024 * 1024 * 1024; break;
            }
            
            if ( multipl > 1 )
            {
                char * src = string_dup( s, l - 1 );
                if ( src != NULL )
                {
                    char * endptr;
                    *value = strtol( src, &endptr, 0 ) * multipl;
                    free( src );
                }
                else
                    *value = dflt;
            }
            else
            {
                char * endptr;
                *value = strtol( s, &endptr, 0 );
            }
        }
    }
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
        rc = ArgsParamValue( args, 0, (const void **)&ctx->url );
        if ( rc == 0 )
        {
            if ( count > 1 )
                rc = ArgsParamValue( args, 1, (const void **)&ctx->destination );
            else
                ctx->destination = NULL;
        }
    }

    if ( rc == 0 ) rc = get_bool( args, OPTION_VERB, &ctx->verbose );
    if ( rc == 0 ) rc = get_bool( args, OPTION_SHOW, &ctx->show_filesize );
    if ( rc == 0 ) rc = get_str( args, OPTION_CACHE, &ctx->cache_file );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_CACHE_BLK, &ctx->cache_blk, 0 );
    if ( rc == 0 ) rc = get_str( args, OPTION_PROXY, &ctx->proxy );
    if ( rc == 0 ) rc = get_bool( args, OPTION_RAND, &ctx->random );
    if ( rc == 0 ) rc = get_bool( args, OPTION_REP, &ctx->with_repeats );
    if ( rc == 0 ) rc = get_bool( args, OPTION_CREPORT, &ctx->report_cache );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_BLOCK, &ctx->blocksize, ( 32 * 1024 ) );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_BUFFER, &ctx->buffer_size, 0 );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_SLEEP, &ctx->sleep_time, 0 );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_TIMEOUT, &ctx->timeout_time, 0 );
    if ( rc == 0 ) rc = get_bool( args, OPTION_COMPLETE, &ctx->check_cache_complete );
    if ( rc == 0 ) rc = get_bool( args, OPTION_CCOMPL, &ctx->check_completeness );
    if ( rc == 0 ) rc = get_bool( args, OPTION_TRUNC, &ctx->truncate_cache );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_START, &ctx->start, 0 );
    if ( rc == 0 ) rc = get_size_t( args, OPTION_COUNT, &ctx->count, 0 );
    if ( rc == 0 ) rc = get_bool( args, OPTION_PROGRESS, &ctx->show_progress );
    if ( rc == 0 ) rc = get_bool( args, OPTION_RELIABLE, &ctx->reliable );
    if ( rc == 0 ) rc = get_bool( args, OPTION_FULL, &ctx->full_download );
    
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
                    else if ( ctx.check_completeness )
                        rc = check_cache_completeness( dir, &ctx );
                    else if ( ctx.truncate_cache )
                        rc = truncate_cache( dir, &ctx );
                    else if ( ctx.show_filesize )
                        rc = show_size( dir, &ctx );
                    else if ( ctx.full_download )
                        rc = full_download( dir, &ctx );
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
