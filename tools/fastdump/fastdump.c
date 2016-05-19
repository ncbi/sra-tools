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

#include "fastdump.vers.h"

#include "cmn_iter.h"
#include "file_printer.h"
#include "raw_read_iter.h"
#include "special_iter.h"
#include "fastq_iter.h"
#include "lookup_writer.h"
#include "lookup_reader.h"
#include "join.h"
#include "sorter.h"
#include "helper.h"

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/out.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <kfs/directory.h>
#include <kproc/thread.h>

#include <os-native.h>
#include <sysalloc.h>

static const char * lookup_usage[] = { "lookup file", NULL };
#define OPTION_LOOKUP   "lookup"
#define ALIAS_LOOKUP    "l"

static const char * range_usage[] = { "row-range", NULL };
#define OPTION_RANGE    "range"
#define ALIAS_RANGE     "R"

static const char * format_usage[] = { "format (special, fastq, lookup, default=special)", NULL };
#define OPTION_FORMAT   "format"
#define ALIAS_FORMAT     "f"

static const char * output_usage[] = { "output-file", NULL };
#define OPTION_OUTPUT   "out"
#define ALIAS_OUTPUT    "o"

static const char * progress_usage[] = { "show progress", NULL };
#define OPTION_PROGRESS "progress"
#define ALIAS_PROGRESS  "p"

static const char * bufsize_usage[] = { "size of file-buffer ( default=1MB )", NULL };
#define OPTION_BUFSIZE  "bufsize"
#define ALIAS_BUFSIZE   "b"

static const char * curcache_usage[] = { "size of cursor-cache ( default=10MB )", NULL };
#define OPTION_CURCACHE "curcache"
#define ALIAS_CURCACHE  "c"

static const char * mem_usage[] = { "memory limit for sorting ( default=2GB )", NULL };
#define OPTION_MEM      "mem"
#define ALIAS_MEM       "m"

static const char * temp_usage[] = { "where to put temp. files ( default=curr. dir )", NULL };
#define OPTION_TEMP     "temp"
#define ALIAS_TEMP      "t"

static const char * threads_usage[] = { "how many thread ( default=1 )", NULL };
#define OPTION_THREADS  "threads"
#define ALIAS_THREADS   "e"

static const char * index_usage[] = { "name of index-file", NULL };
#define OPTION_INDEX    "index"
#define ALIAS_INDEX     "i"

OptDef ToolOptions[] =
{
    { OPTION_RANGE,     ALIAS_RANGE,     NULL, range_usage,      1, true,   false },
    { OPTION_LOOKUP,    ALIAS_LOOKUP,    NULL, lookup_usage,     1, true,   false },
    { OPTION_FORMAT,    ALIAS_FORMAT,    NULL, format_usage,     1, true,   false },
    { OPTION_OUTPUT,    ALIAS_OUTPUT,    NULL, output_usage,     1, true,   false },
    { OPTION_BUFSIZE,   ALIAS_BUFSIZE,   NULL, bufsize_usage,    1, true,   false },
    { OPTION_CURCACHE,  ALIAS_CURCACHE,  NULL, curcache_usage,   1, true,   false },
    { OPTION_MEM,       ALIAS_MEM,       NULL, mem_usage,        1, true,   false },
    { OPTION_TEMP,      ALIAS_TEMP,      NULL, temp_usage,       1, true,   false },
    { OPTION_THREADS,   ALIAS_THREADS,   NULL, threads_usage,    1, true,   false },
    { OPTION_INDEX,     ALIAS_INDEX,     NULL, index_usage,      1, true,   false },
    { OPTION_PROGRESS,  ALIAS_PROGRESS,  NULL, progress_usage,   1, false,  false }
};

const char UsageDefaultName[] = "fastdump";

rc_t CC UsageSummary( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [options]\n"
                     "\n", progname );
}


rc_t CC Usage ( const Args * args )
{
    rc_t rc;
    uint32_t idx, count = ( sizeof ToolOptions ) / ( sizeof ToolOptions[ 0 ] );
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;

    if ( args == NULL )
        rc = RC( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary( progname );

    KOutMsg( "Options:\n" );
    for ( idx = 1; idx < count; ++idx ) /* start with 1, do not advertize row-range-option*/
        HelpOptionLine( ToolOptions[ idx ].aliases, ToolOptions[ idx ].name, NULL, ToolOptions[ idx ].help );
    
    HelpOptionsStandard();
    HelpVersion( fullpath, KAppVersion() );
    return rc;
}


/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion( void ) { return FASTDUMP_VERS; }


/* -------------------------------------------------------------------------------------------- */

typedef struct fd_ctx
{
    cmn_params cmn;
    const char * lookup_filename;
    const char * output_filename;
    const char * index_filename;
    const char * temp_path;
    size_t buf_size, mem_limit;
    uint64_t num_threads;
} fd_ctx;


static void init_sorter_params( const fd_ctx * fd_ctx, sorter_params * sp )
{
    sp->dir = fd_ctx->cmn.dir;
    sp->acc = fd_ctx->cmn.acc;
    sp->output_filename = fd_ctx->output_filename;
    sp->index_filename = fd_ctx->index_filename;
    sp->temp_path = fd_ctx->temp_path;
    sp->src = NULL; /* sorter takes ownership! */
    sp->prefix = 0;
    sp->mem_limit = fd_ctx->mem_limit;
    sp->buf_size = fd_ctx->buf_size;
    sp->cursor_cache = fd_ctx->cmn.cursor_cache;
    sp->sort_progress = NULL;
    sp->num_threads = 0;
    sp->show_progress = fd_ctx->cmn.show_progress;
}

/* --------------------------------------------------------------------------------------------
    produce the lookup-table by iterating over the PRIMARY_ALIGNMENT - table:
   -------------------------------------------------------------------------------------------- 
    reading SEQ_SPOT_ID, SEQ_READ_ID and RAW_READ
    SEQ_SPOT_ID and SEQ_READ_ID is merged into a 64-bit-key
    RAW_READ is read as 4na-unpacked ( Schema does not provide 4na-packed for this column )
    these key-pairs are temporarely stored in a KVector until a limit is reached
    after that limit is reached they are writen sorted into the file-system as sub-files
    this repeats until the requested row-range is exhausted ( row_range ... NULL -> all rows )
    These sub-files are than merge-sorted into the final output-file.
    This output-file is a binary data-file:
    content: [KEY][RAW_READ]
    KEY... 64-bit value as SEQ_SPOT_ID shifted left by 1 bit, zero-bit contains SEQ_READ_ID
    RAW_READ... 16-bit binary-chunk-lenght, followed by n bytes of packed 4na
-------------------------------------------------------------------------------------------- */
static rc_t single_threaded_make_lookup( fd_ctx * fd_ctx )
{
    sorter_params sp;
    struct raw_read_iter * iter;
    
    init_sorter_params( fd_ctx, &sp );
    rc_t rc = make_raw_read_iter( &fd_ctx->cmn, &iter );
    if ( rc == 0 )
    {
        sp.src = iter; /* sorter takes ownership! */
        rc = run_sorter( &sp ); /* sorter.c */
    }
    return rc;
}


static rc_t multi_threaded_make_lookup( fd_ctx * fd_ctx )
{
    sorter_params sp;

    init_sorter_params( fd_ctx, &sp );
    sp.num_threads = fd_ctx->num_threads;
    return run_sorter_pool( &sp ); /* sorter.c */
}


/* --------------------------------------------------------------------------------------------
    produce special-output ( SPOT_ID,READ,SPOT_GROUP ) by iterating over the SEQUENCE - table:
    produce fastq-output by iterating over the SEQUENCE - table:
   -------------------------------------------------------------------------------------------- 
   
-------------------------------------------------------------------------------------------- */

static rc_t perform_join( fd_ctx * fd_ctx, format_t fmt )
{
    rc_t rc = 0;
    if ( !file_exists( fd_ctx->cmn.dir, "%s", fd_ctx->lookup_filename ) )
    {
        const char * temp = fd_ctx->output_filename;
        
        if ( fd_ctx->cmn.show_progress )
            KOutMsg( "lookup :" );
        
        fd_ctx->output_filename = fd_ctx->lookup_filename;
        if ( fd_ctx->num_threads > 1 )
            rc = multi_threaded_make_lookup( fd_ctx );
        else
            rc = single_threaded_make_lookup( fd_ctx );

        fd_ctx->output_filename = temp;
    }
    
    if ( rc == 0 )
    {
        join_params jp;
        
        jp.dir              = fd_ctx->cmn.dir;
        jp.accession        = fd_ctx->cmn.acc;
        jp.lookup_filename  = fd_ctx->lookup_filename;
        jp.index_filename   = fd_ctx->index_filename;
        jp.output_filename  = fd_ctx->output_filename;
        jp.temp_path        = fd_ctx->temp_path;
        jp.join_progress    = NULL;
        jp.buf_size         = fd_ctx->buf_size;
        jp.cur_cache        = fd_ctx->cmn.cursor_cache;
        jp.show_progress    = fd_ctx->cmn.show_progress;
        jp.num_threads      = fd_ctx->num_threads;
        jp.first            = 0;
        jp.count            = 0;
        jp.fmt              = fmt;
        
        rc = execute_join( &jp ); /* join.c */
    }
    return rc;
}


/* -------------------------------------------------------------------------------------------- */

rc_t CC KMain ( int argc, char *argv [] )
{
    rc_t rc;
    Args * args;
    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];

    rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( rc != 0 )
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    if ( rc == 0 )
    {
        fd_ctx fd_ctx;
        rc = ArgsParamValue( args, 0, (const void **)&fd_ctx.cmn.acc );
        if ( rc != 0 )
            ErrMsg( "ArgsParamValue() -> %R", rc );
        else
        {
            const char * format = get_str_option( args, OPTION_FORMAT, NULL );
            format_t fmt = get_format_t( format );
            char dflt_lookup[ 4096 ];
            char dflt_index[ 4096 ];
            char dflt_output[ 4096 ];
            
            dflt_lookup[ 0 ] = 0;
            dflt_index[ 0 ] = 0;
            dflt_output[ 0 ] = 0;
            
            fd_ctx.cmn.row_range = get_str_option( args, OPTION_RANGE, NULL );
            fd_ctx.cmn.cursor_cache = get_size_t_option( args, OPTION_CURCACHE, 5 * 1024 * 1024 );            
            fd_ctx.cmn.show_progress = get_bool_option( args, OPTION_PROGRESS );
            fd_ctx.cmn.count = 0;

            fd_ctx.temp_path = get_str_option( args, OPTION_TEMP, NULL );
            fd_ctx.output_filename = get_str_option( args, OPTION_OUTPUT, NULL );
            fd_ctx.lookup_filename = get_str_option( args, OPTION_LOOKUP, NULL );            
            fd_ctx.index_filename = get_str_option( args, OPTION_INDEX, NULL );
            fd_ctx.buf_size = get_size_t_option( args, OPTION_BUFSIZE, 1024 * 1024 );
            fd_ctx.mem_limit = get_size_t_option( args, OPTION_MEM, 1024L * 1024 * 100 );
            fd_ctx.num_threads = get_uint64_t_option( args, OPTION_THREADS, 1 );

            if ( fd_ctx.lookup_filename == NULL )
            {
                rc = make_prefixed( dflt_lookup, sizeof dflt_lookup, fd_ctx.temp_path,
                                    fd_ctx.cmn.acc, ".lookup" );
                if ( rc == 0 )
                    fd_ctx.lookup_filename = dflt_lookup;
            }

            if ( fd_ctx.index_filename == NULL )
            {
                rc = make_prefixed( dflt_index, sizeof dflt_index, fd_ctx.temp_path,
                                    fd_ctx.cmn.acc, ".lookup.idx" );
                if ( rc == 0 )
                    fd_ctx.index_filename = dflt_index;
            }

            if ( fd_ctx.output_filename == NULL )
            {
                rc = make_prefixed( dflt_output, sizeof dflt_output, NULL,
                                    fd_ctx.cmn.acc, ".txt" );
                if ( rc == 0 )
                    fd_ctx.output_filename = dflt_output;
            }

            rc = KDirectoryNativeDir( &fd_ctx.cmn.dir );
            if ( rc != 0 )
                ErrMsg( "KDirectoryNativeDir() -> %R", rc );
            else
            {
                rc = perform_join( &fd_ctx, fmt );

                if ( dflt_lookup[ 0 ] != 0 )
                    KDirectoryRemove( fd_ctx.cmn.dir, true, "%s", dflt_lookup );

                if ( dflt_index[ 0 ] != 0 )
                    KDirectoryRemove( fd_ctx.cmn.dir, true, "%s", dflt_index );
                
                KDirectoryRelease( fd_ctx.cmn.dir );
            }
        }
    }
    return rc;
}
