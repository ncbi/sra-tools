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

#include "refseq-load.vers.h"
#include "refseq-load-includes.h"
#include "context.h"

#include <kapp/main.h>

#ifndef _h_align_writer_refseq_
 #include <align/writer-refseq.h>
#endif
/*
#include <kns/url-fetcher.h>
#include <kns/entrez-fetcher.h>
*/

#include <sysalloc.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include <stdlib.h>
#include <string.h>

static const char * src_usage[] = { "full url or name of accession", NULL };
static const char * dst_usage[] = { "name of the table to write", NULL };
static const char * force_usage[] = { "force target overwrite ", NULL };
static const char * schema_usage[] = { "name of the schema-file for dst-table", NULL };
static const char * chunk_size_usage[] = { "size of the seq-chunk (default = 5k)", NULL };
static const char * circular_usage[] = { "indicate if refseq is circular (ex: mitochondria)", NULL };
static const char * ifile_usage[] = { "treats input-obj as filename not url", NULL };

OptDef MyOptions[] =
{                                                                       /* needs_value, required */
    { OPTION_SRC,           ALIAS_SRC,          NULL, src_usage,        1,  true,   false },
    { OPTION_DST_PATH,      ALIAS_DST_PATH,     NULL, dst_usage,        1,  true,   false },
    { OPTION_FORCE,         ALIAS_FORCE,        NULL, force_usage,      1,  false,  false },
    { OPTION_SCHEMA,        ALIAS_SCHEMA,       NULL, schema_usage,     1,  true,   false },
    { OPTION_CHUNK_SIZE,    ALIAS_CHUNK_SIZE,   NULL, chunk_size_usage, 1,  true,   false },
    { OPTION_CIRCULAR,      ALIAS_CIRCULAR,     NULL, circular_usage,   1,  true,   true  },
    { OPTION_IFILE,         ALIAS_IFILE,        NULL, ifile_usage,      1,  false,  false }
};

const char UsageDefaultName [] = "refseq-load";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s [options] -f <url | accession> -d <dst_path> -c no\n"
        "\n", progname );
}


rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    KOutMsg ( "Options:\n" );


    HelpOptionLine ( ALIAS_SRC, OPTION_SRC, "src", src_usage );
    HelpOptionLine ( ALIAS_DST_PATH, OPTION_DST_PATH, "dst", dst_usage );
    HelpOptionLine ( ALIAS_CIRCULAR, OPTION_CIRCULAR, "yes|no", circular_usage);

    KOutMsg ( "\nOptions:\n" );
    HelpOptionLine ( ALIAS_SCHEMA, OPTION_SCHEMA, "schema", schema_usage );
    HelpOptionLine ( ALIAS_CHUNK_SIZE, OPTION_CHUNK_SIZE, "chunk-size", chunk_size_usage );
    HelpOptionLine ( ALIAS_IFILE, OPTION_IFILE, "fileinput", ifile_usage);
    KOutMsg( "\n" );

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
    return REFSEQ_LOAD_VERS;
}


static bool is_full_url( const char * s )
{
    return strchr(s, '/' ) != NULL;
}


static rc_t build_full_url( char ** s )
{
    rc_t rc;
    String prefix, accession;
    const String * full_url;

    StringInitCString( &prefix, ENTREZ_URL );
    StringInitCString( &accession, *s );
    rc = StringConcat ( &full_url, &prefix, &accession );
    if ( rc == 0 )
    {
        free( *s );
        *s = string_dup_measure ( full_url->addr, NULL );
        StringWhack ( full_url );
    }
    return rc;
}


typedef struct data_ctx
{
    char *buffer;
    bool first_line;
    bool quiet;
    size_t buff_size, data_count;
    int64_t rowid;
    uint64_t byte_count;
    const TableWriterRefSeq* wr;
    rc_t rc;
} data_ctx;
typedef data_ctx* p_data_ctx;


static rc_t write_default_uint32( const uint32_t value,
                           enum ETableWriterRefSeq_ColNames which_col,
                           const TableWriterRefSeq* wr )
{
    rc_t rc;
    TableWriterData data;
    data.buffer = &value;
    data.elements = 1;
    rc = TableWriterRefSeq_WriteDefault( wr, which_col, &data );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "write_default_uint32:TableWriterRefSeq_WriteDefault() failed" );
    }
    return rc;
}


static rc_t write_default_str( const char * value,
                           enum ETableWriterRefSeq_ColNames which_col,
                           const TableWriterRefSeq* wr )
{
    rc_t rc;
    TableWriterData data;
    data.buffer = (void*)value;
    data.elements = strlen( value );
    rc = TableWriterRefSeq_WriteDefault( wr, which_col, &data );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "write_default_str:TableWriterRefSeq_WriteDefault() failed" );
    }
    return rc;
}


static rc_t write_default_bool( const bool value,
                                enum ETableWriterRefSeq_ColNames which_col,
                                const TableWriterRefSeq* wr )
{
    rc_t rc;
    TableWriterData data;
    data.buffer = &value;
    data.elements = 1;
    rc = TableWriterRefSeq_WriteDefault( wr, which_col, &data );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "write_default_bool:TableWriterRefSeq_WriteDefault() failed" );
    }
    return rc;
}


static void write_curl_data( p_data_ctx dctx )
{
    TableWriterRefSeqData data;

    data.read.buffer = dctx->buffer;
    data.read.elements = dctx->data_count;
    dctx->rc = TableWriterRefSeq_Write( dctx->wr, &data, &dctx->rowid );
    if ( dctx->rc != 0 )
    {
        LOGERR( klogErr, dctx->rc, "handle_curl_byte:TableWriterRefSeq_Write() failed" );
    }

    if ( ( !dctx->quiet ) && ( dctx->rowid % 100 == 0 ) )
    {
        KOutMsg( "row %ld\r", dctx->rowid );
    }

    dctx->data_count = 0;
}


char * find_space( const char * s )
{
    char *res = ( char * )s;
    while ( *res != 0 )
        if ( *res == ' ' )
            return res;
        else
            res++;
    return NULL;
}


static bool check_first_line( const char * s )
{
    if ( s == NULL ) return false;
    if ( s[ 0 ] == 0 ) return false;
    return ( s[ 0 ] == '>' );
}


static bool handle_first_line( p_data_ctx dctx )
{
    char *space, *s = ( char * )dctx->buffer;
    if ( !check_first_line( s ) )
        return false;

    if ( *s == '>' ) s++;
    space = find_space( s );
    if ( space == NULL ) {
        space = s + strlen(s);
    } else {
        *space = 0;
    }
    if ( !dctx->quiet )
        KOutMsg( "SEQ_ID: %s\n", s );
    write_default_str( s, ewrefseq_cn_SEQ_ID, dctx->wr );
    space++;
    if ( !dctx->quiet )
        KOutMsg( "DEF_LINE: %s\n", space );
    write_default_str( space, ewrefseq_cn_DEF_LINE, dctx->wr );
    return true;
}


static void handle_curl_byte( const char c, p_data_ctx dctx )
{
    if ( dctx->first_line )
    {
        if ( c == 0x0A )
        {
            dctx->buffer[ dctx->data_count++ ] = 0;
            if ( !handle_first_line( dctx ) )
            {
                dctx->rc = RC(rcExe, rcBuffer, rcAccessing, rcData, rcInvalid );
                LOGERR( klogErr, dctx->rc, "server returned invalid data, possibly wrong accession/url" );
            }
            dctx->data_count = 0;
            dctx->first_line = false;
        }
        else
        {
            if ( dctx->data_count < dctx->buff_size )
                dctx->buffer[ dctx->data_count++ ] = c;
        }
    }
    else
    {
        if ( c > 0x20 )
        {
            dctx->byte_count++;
            dctx->buffer[ dctx->data_count++ ] = c;
            if ( dctx->data_count >= dctx->buff_size )
                write_curl_data( dctx );
        }
    }
}


static size_t CC on_curl_data( void *ptr, size_t size, size_t nmemb, void *data )
{
    size_t given_bytes = size * nmemb; /* calculate the size given in ptr */
    p_data_ctx dctx = ( p_data_ctx )data;
    if ( dctx != NULL )
    {
        size_t idx;

        /* to be able to cancel the loop by signal */
        dctx->rc = Quitting();
        for ( idx = 0; idx < given_bytes && dctx->rc == 0; ++idx )
        {
            handle_curl_byte( ((char *)ptr)[idx], dctx );
        }

        /* tell curl via return value to stop downloading,
           if we encountered an error or quit */
        if ( dctx->rc != 0 )
            given_bytes = 0;
    }
    return given_bytes;
}


static rc_t init_data_context( const p_context ctx, const TableWriterRefSeq* wr, data_ctx * dctx )
{
    rc_t rc = 0;
    dctx->buff_size = ctx->chunk_size;
    dctx->data_count = 0;
    dctx->first_line = true;
    dctx->quiet = ctx->quiet;
    dctx->wr = wr;
    dctx->rc = 0;
    dctx->rowid = 0;
    dctx->byte_count = 0;
    dctx->buffer = malloc( dctx->buff_size );
    if ( dctx->buffer == NULL )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcMemory, rcExhausted );
        LOGERR( klogErr, rc, "creating receive-buffer failed" );
    }
    return rc;
}


static void finish_data_context( const p_context ctx, data_ctx * dctx )
{
    /* write the remainder of data that is eventually left 
      in the buffer by the callback */
    if ( dctx->data_count > 0 )
    {
        /* returns the rc-code in dctx.rc, this function will return this code at the end */
        write_curl_data( dctx );
    }

    if ( !ctx->quiet )
        KOutMsg( "%ld bases\n", dctx->byte_count );
    free( dctx->buffer );
}


static rc_t perform_curl_load( const p_context ctx, const TableWriterRefSeq* wr )
{
    CURL *curl_handle;
    CURLcode rcc;
    data_ctx dctx;
    rc_t rc = 0;

    curl_handle = curl_easy_init();
    if ( curl_handle == NULL )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcData, rcInvalid );
        LOGERR( klogErr, rc, "cur_easy_init() failed" );
        return rc;
    }

#ifdef _DEBUGGING
    if( KDbgTestModConds( DBG_APP, DBG_FLAG_ANY ) )
    {
        curl_easy_setopt( curl_handle, CURLOPT_VERBOSE , 1 );
    }
#endif

    rc = init_data_context( ctx, wr, &dctx );
    if ( rc != 0 )
    {
        curl_easy_cleanup( curl_handle );
        return rc;
    }

    rcc = curl_easy_setopt( curl_handle, CURLOPT_WRITEFUNCTION, on_curl_data );
    if ( rcc != CURLE_OK )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcParam, rcInvalid );
        LOGERR( klogErr, rc, "curl_easy_setopt(callback) failed" );
        free( dctx.buffer );
        curl_easy_cleanup( curl_handle );
        return rc;
    }

    rcc = curl_easy_setopt( curl_handle, CURLOPT_WRITEDATA, (void*)&dctx );
    if ( rcc != CURLE_OK )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcParam, rcInvalid );
        LOGERR( klogErr, rc, "curl_easy_setopt(context) failed" );
        free( dctx.buffer );
        curl_easy_cleanup( curl_handle );
        return rc;
    }

    rcc = curl_easy_setopt( curl_handle, CURLOPT_URL, ctx->src );
    if ( rcc != CURLE_OK )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcParam, rcInvalid );
        LOGERR( klogErr, rc, "curl_easy_setopt(uri) failed" );
        free( dctx.buffer );
        curl_easy_cleanup( curl_handle );
        return rc;
    }

    rcc = curl_easy_setopt( curl_handle, CURLOPT_LOW_SPEED_LIMIT, 1000 );
    if ( rcc != CURLE_OK )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcParam, rcInvalid );
        LOGERR( klogErr, rc, "curl_easy_setopt(CURLOPT_LOW_SPEED_LIMIT) to 1000 failed" );
        free( dctx.buffer );
        curl_easy_cleanup( curl_handle );
        return rc;
    }

    rcc = curl_easy_setopt( curl_handle, CURLOPT_LOW_SPEED_TIME, 5 );
    if ( rcc != CURLE_OK )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcParam, rcInvalid );
        LOGERR( klogErr, rc, "curl_easy_setopt(CURLOPT_LOW_SPEED_TIME) to 5 failed" );
        free( dctx.buffer );
        curl_easy_cleanup( curl_handle );
        return rc;
    }

    rcc = curl_easy_perform( curl_handle );
    if ( rcc != CURLE_OK )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcParam, rcInvalid );
        LOGERR( klogErr, rc, "curl_easy_perform() failed" );
    }

    finish_data_context( ctx, &dctx );
    curl_easy_cleanup( curl_handle );
    if ( dctx.rc == 0 )
    {
        dctx.rc = rc;
    }
    return dctx.rc;
}


static rc_t perform_file_load( const p_context ctx, KDirectory *dir, const TableWriterRefSeq* wr )
{
    rc_t rc = 0;
    uint32_t pt = KDirectoryPathType ( dir, "%s", ctx->src );
    if ( pt != kptFile )
    {
        rc = RC( rcExe, rcFile, rcConstructing, rcParam, rcInvalid );
        LOGERR( klogErr, rc, "input-file not found" );
    }
    else
    {
        const KFile * f;
        rc = KDirectoryOpenFileRead ( dir, &f, "%s", ctx->src );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "cannot open input-file for read" );
        }
        else
        {
            data_ctx dctx;
            rc = init_data_context( ctx, wr, &dctx );
            if ( rc == 0 )
            {
                uint64_t pos = 0;
                bool done = false;
                while ( !done )
                {
                    size_t num_read;
                    char buffer[ 8192 ];
                    rc = KFileReadAll ( f, pos, buffer, sizeof buffer, &num_read );
                    if ( rc != 0 )
                    {
                        PLOGERR( klogErr, ( klogErr, rc, "$(path) at $(pos)", "path=%s,pos=%lu", ctx->src, pos ) );
                        done = true;
                    }
                    else
                    {
                        on_curl_data( buffer, 1, num_read, &dctx );

                        if ( num_read < sizeof( buffer ) )
                            done = true;
                        else
                            pos += num_read;
                    }
                }
                finish_data_context( ctx, &dctx );
                if ( dctx.rc == 0 )
                {
                    dctx.rc = rc;
                }
            }
            KFileRelease( f );
        }
    }
    return rc;
}


static rc_t perform_load( const p_context ctx, KDirectory *dir, VDBManager *mgr )
{
    const TableWriterRefSeq * wr;

    rc_t rc = TableWriterRefSeq_Make( &wr, mgr, ctx->schema, ctx->dst_path, 0 );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "perform_load:TableWriterRefSeq_Make() failed" );
    }
    else
    {
        rc_t rc1;
        uint64_t rows;

        if ( ctx->chunk_size != TableWriterRefSeq_MAX_SEQ_LEN )
        {
            rc = write_default_uint32( ctx->chunk_size,
                                       ewrefseq_cn_MAX_SEQ_LEN, wr );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "perform_load:write_default_uint32() failed" );
            }
        }

        if ( rc == 0 )
        {
            rc = write_default_bool( ctx->circular, ewrefseq_cn_CIRCULAR, wr );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "perform_load:write_default_bool() failed" );
            }
        }

        if ( rc == 0 )
        {
            if ( ctx->input_is_file )
            {
                rc = perform_file_load( ctx, dir, wr ); /* <====================== */
                if ( rc != 0 )
                {
                    LOGERR( klogErr, rc, "perform_load:perform_file_load() failed" );
                }
            }
            else
            {
                rc = perform_curl_load( ctx, wr ); /* <====================== */
                if ( rc != 0 )
                {
                    LOGERR( klogErr, rc, "perform_load:perform_curl_load() failed" );
                }
            }
        }

        rc1 = TableWriterRefSeq_Whack( wr, rc == 0, &rows, ctx->argv0, KAppVersion(), __DATE__, "RefSeq", REFSEQ_LOAD_VERS );
        if ( rc == 0 )
        {
            if ( rc1 != 0 )
            {
                LOGERR( klogErr, rc1, "perform_load:TableWriterRefSeq_Whack() failed" );
            }
            rc = rc1;
        }

        if ( !ctx->quiet )
            KOutMsg( "%lu rows were written\n", rows );
    }
    return rc;
}


static rc_t check_if_schema_exists( KDirectory * dir, const char * schema_name )
{
    KConfig * cfg;
    rc_t rc = KConfigMake ( &cfg, NULL );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "check_if_schema_exists:KConfigMake() failed" );
    }
    else
    {
        const KConfigNode *node;
        rc = KConfigOpenNodeRead ( cfg, &node, "vdb/schema/paths" );
        if ( rc !=  0 )
        {
            LOGERR( klogErr, rc, "check_if_schema_exists:KConfigOpenNodeRead( 'vdb/schema/paths' ) failed" );
        }
        else
        {
            char buffer[ 1024 ];
            size_t num_read;
            rc = KConfigNodeRead ( node, 0, buffer, sizeof buffer, &num_read, NULL );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "check_if_schema_exists:KConfigNodeRead() failed" );
            }
            else
            {
                uint32_t path_type;
                buffer[ num_read ] = 0;
                path_type = KDirectoryPathType ( dir, "%s/%s", buffer, schema_name );
                if ( ( path_type & kptFile ) == 0 )
                {
                    rc = RC( rcExe, rcFile, rcConstructing, rcParam, rcInvalid );
                }
            }
            KConfigNodeRelease ( node );
        }
        KConfigRelease ( cfg );
    }
    return rc;
}


static rc_t prepare_load( KDirectory * dir, const p_context ctx )
{
    rc_t rc = 0;

    if ( !ctx->input_is_file )
    {
        /* if the input is not specified as a file, we check for url and correctness */
        if ( !is_full_url( ctx->src ) )
        {
            if ( !ctx->quiet )
                KOutMsg( "src is not a full url, assuming it is a accession\n" );
            rc = build_full_url( &ctx->src );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "prepare_load:build_full_url() failed" );
            }
        }
    }

    /* and then we look if the given schema does exist */
    if ( rc == 0 )
    {
        if ( ctx->schema == NULL )
        {
            ctx->schema = string_dup_measure ( DEFAULT_SCHEMA, NULL );
        }
        if ( ctx->chunk_size == 0 )
        {
            ctx->chunk_size = TableWriterRefSeq_MAX_SEQ_LEN;
        }

        rc = check_if_schema_exists( dir, ctx->schema );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "check_if_schema_exists() failed" );
        }
        else if ( !ctx->quiet )
        {
            KOutMsg( "src        = '%s'\n", ctx->src );
            KOutMsg( "dst        = '%s'\n", ctx->dst_path );
            KOutMsg( "schema     = '%s'\n", ctx->schema );
            KOutMsg( "chunk-size = %u\n",   ctx->chunk_size );
            KOutMsg( "srs is file= %s\n",   ctx->input_is_file ? "yes" : "no" );
        }
    }
    return rc;
}


static rc_t remove_path( KDirectory * dir, const char * path, bool quiet )
{
    rc_t rc;

    if ( !quiet )
    {
        PLOGMSG( klogInfo, ( klogInfo, "removing '$(path)'", "path=%s", path ));
    }
    rc = KDirectoryRemove ( dir, true, "%s", path );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "remove_path:KDirectoryRemove() failed" );
    }
    return rc;
}


static rc_t ref_seq_load_main( const p_context ctx )
{
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "ref_seq_load_main:KDirectoryNativeDir() failed" );
    }
    else
    {
        VDBManager *mgr;
        rc = VDBManagerMakeUpdate ( &mgr, dir );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "ref_seq_load_main:VDBManagerMakeRead() failed" );
        }
        else
        {
            rc = prepare_load( dir, ctx );
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "ref_seq_load_main:prepare_load() failed" );
            }
            else
            {
                char tblpath[ 4096 ];
                rc = KDirectoryResolvePath( dir, true, tblpath, sizeof tblpath, "%s", ctx->dst_path );
                if ( rc != 0 )
                {
                    LOGERR( klogErr, rc, "ref_seq_load_main:KDirectoryResolvePath() failed" );
                }
                else
                {
                    KPathType type = VDBManagerPathType( mgr, "%s", tblpath );
                    if ( type != kptNotFound )
                    {
                        rc = RC( rcExe, rcDatabase, rcCreating, rcFile, rcExists );
                        PLOGERR( klogErr, ( klogErr, rc, "$(path)", "path=%s", tblpath ) );
                    }
                }
            }

            if ( rc == 0 )
            {
                rc = perform_load( ctx, dir, mgr ); /* <====================== */
                if ( rc != 0 )
                {
                    remove_path( dir, ctx->dst_path, ctx->quiet );
                }
            }

            VDBManagerRelease( mgr );
        }
        KDirectoryRelease( dir );
    }
    return rc;
}


/***************************************************************************
    Main:
    * create the copy-context
    * parse the commandline for arguments and options
    * react to help/usage - requests ( no dump in this case )
      these functions are in vdb-copy-context.c
    * call copy_main() to execute the copy-operation
    * destroy the copy-context
***************************************************************************/
rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                                  MyOptions, sizeof MyOptions / sizeof ( OptDef ) );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "ArgsMakeAndHandle() failed" );
    }
    else
    {
        context *ctx;

        KLogHandlerSetStdErr();
        rc = context_init( &ctx );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "KMain:context_init() failed" );

        }
        else
        {
            rc = context_capture_arguments_and_options( args, ctx );
            if ( rc != 0 )
            {
                MiniUsage( args );
                if ( argc < 2 ) rc = 0;
            }
            else
            {
                if ( ctx->usage_requested )
                    MiniUsage( args );
                else
                    rc = ref_seq_load_main( ctx ); /* <====================== */
            }
            context_destroy( ctx );
        }
        ArgsWhack ( args );
    }
    return rc;
}
