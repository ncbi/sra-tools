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

static const char * mem_usage[] = { "memory limit for sorting ( default=100MB )", NULL };
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

static const char * detail_usage[] = { "print details", NULL };
#define OPTION_DETAILS  "details"
#define ALIAS_DETAILS    "x"

static const char * split_usage[] = { "split fastq-output", NULL };
#define OPTION_SPLIT     "split"
#define ALIAS_SPLIT      "s"

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
    { OPTION_PROGRESS,  ALIAS_PROGRESS,  NULL, progress_usage,   1, false,  false },
    { OPTION_DETAILS,   ALIAS_DETAILS,   NULL, detail_usage,     1, false,  false },
    { OPTION_SPLIT,     ALIAS_SPLIT,     NULL, split_usage,      1, false,  false }    
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
        HelpOptionLine( ToolOptions[ idx ] . aliases, ToolOptions[ idx ] . name, NULL, ToolOptions[ idx ] . help );
    
    HelpOptionsStandard();
    HelpVersion( fullpath, KAppVersion() );
    return rc;
}

/* -------------------------------------------------------------------------------------------- */

typedef struct tool_ctx
{
    cmn_params cmn;
    const char * lookup_filename;
    const char * output_filename;
    const char * index_filename;
    const char * temp_path;
    bool remove_temp_path;
    size_t buf_size, mem_limit;
    uint64_t num_threads;
    format_t fmt;
} tool_ctx;


static void init_sorter_params( const tool_ctx * tool_ctx, sorter_params * sp )
{
    sp->dir = tool_ctx -> cmn.dir;
    sp->acc = tool_ctx -> cmn.acc;
    sp->output_filename = tool_ctx -> output_filename;
    sp->index_filename = tool_ctx -> index_filename;
    sp->temp_path = tool_ctx -> temp_path;
    sp->src = NULL; /* sorter takes ownership! */
    sp->prefix = 0;
    sp->mem_limit = tool_ctx -> mem_limit;
    sp->buf_size = tool_ctx -> buf_size;
    sp->cursor_cache = tool_ctx -> cmn . cursor_cache;
    sp->sort_progress = NULL;
    sp->num_threads = 0;
    sp->show_progress = tool_ctx -> cmn . show_progress;
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
static rc_t single_threaded_make_lookup( tool_ctx * tool_ctx )
{
    sorter_params sp;
    struct raw_read_iter * iter;
    
    init_sorter_params( tool_ctx, &sp );
    rc_t rc = make_raw_read_iter( &tool_ctx -> cmn, &iter );
    if ( rc == 0 )
    {
        sp . src = iter; /* sorter takes ownership! */
        rc = run_sorter( &sp ); /* sorter.c */
    }
    return rc;
}


static rc_t multi_threaded_make_lookup( tool_ctx * tool_ctx )
{
    sorter_params sp;

    init_sorter_params( tool_ctx, &sp );
    sp.num_threads = tool_ctx -> num_threads;
    return run_sorter_pool( &sp ); /* sorter.c */
}


/* --------------------------------------------------------------------------------------------
    produce special-output ( SPOT_ID,READ,SPOT_GROUP ) by iterating over the SEQUENCE - table:
    produce fastq-output by iterating over the SEQUENCE - table:
   -------------------------------------------------------------------------------------------- 
   
-------------------------------------------------------------------------------------------- */

static rc_t perform_join( tool_ctx * tool_ctx )
{
    rc_t rc = 0;
    if ( !file_exists( tool_ctx -> cmn . dir, "%s", tool_ctx -> lookup_filename ) )
    {
        const char * temp = tool_ctx -> output_filename;
        
        if ( tool_ctx -> cmn . show_progress )
            KOutMsg( "lookup :" );
        
        tool_ctx -> output_filename = tool_ctx -> lookup_filename;
        if ( tool_ctx -> num_threads > 1 )
            rc = multi_threaded_make_lookup( tool_ctx ); /* <=== above! */
        else
            rc = single_threaded_make_lookup( tool_ctx ); /* <=== above! */

        tool_ctx -> output_filename = temp;
    }
    
    if ( rc == 0 )
    {
        join_params jp;
        
        jp.dir              = tool_ctx -> cmn . dir;
        jp.accession        = tool_ctx -> cmn . acc;
        jp.lookup_filename  = tool_ctx -> lookup_filename;
        jp.index_filename   = tool_ctx -> index_filename;
        jp.output_filename  = tool_ctx -> output_filename;
        jp.temp_path        = tool_ctx -> temp_path;
        jp.join_progress    = NULL;
        jp.buf_size         = tool_ctx -> buf_size;
        jp.cur_cache        = tool_ctx -> cmn.cursor_cache;
        jp.show_progress    = tool_ctx -> cmn.show_progress;
        jp.num_threads      = tool_ctx -> num_threads;
        jp.first            = 0;
        jp.count            = 0;
        jp.fmt              = tool_ctx -> fmt;
        
        rc = execute_join( &jp ); /* join.c */
    }
    return rc;
}


static rc_t show_details( tool_ctx * tool_ctx )
{
    rc_t rc = KOutMsg( "cursor-cache : %ld\n", tool_ctx -> cmn . cursor_cache );
    if ( rc == 0 )
        rc = KOutMsg( "buf-size     : %ld\n", tool_ctx -> buf_size );
    if ( rc == 0 )
        rc = KOutMsg( "mem-limit    : %ld\n", tool_ctx -> mem_limit );
    if ( rc == 0 )
        rc = KOutMsg( "threads      : %d\n", tool_ctx -> num_threads );
    if ( rc == 0 )
        rc = KOutMsg( "scratch-path : '%s'\n", tool_ctx -> temp_path );
    if ( rc == 0 )
        rc = KOutMsg( "output-format: " );
    if ( rc == 0 )
    {
        switch ( tool_ctx -> fmt )
        {
            case ft_special     : rc = KOutMsg( "SPECIAL\n" ); break;
            case ft_fastq       : rc = KOutMsg( "FASTQ\n" ); break;
            case ft_fastq_split : rc = KOutMsg( "FASTQ splitted\n" ); break;
        }
    }
    return rc;
}

static const char * dflt_temp_path = "./fast.tmp";

static rc_t check_temp_path( tool_ctx * tool_ctx )
{
    rc_t rc = 0;
    uint32_t pt;
    if ( tool_ctx -> temp_path == NULL )
    {
        tool_ctx -> temp_path = dflt_temp_path;
        pt = KDirectoryPathType( tool_ctx -> cmn . dir, "%s", tool_ctx -> temp_path );
        if ( pt != kptDir )
        {
            rc = KDirectoryCreateDir ( tool_ctx -> cmn . dir, 0775, kcmInit, "%s", tool_ctx -> temp_path );
            if ( rc != 0 )
                ErrMsg( "scratch-path '%s' cannot be created!", tool_ctx -> temp_path );
            else
                tool_ctx -> remove_temp_path = true;
        }
    }
    else
    {
        pt = KDirectoryPathType( tool_ctx -> cmn . dir, "%s", tool_ctx -> temp_path );
        if ( pt != kptDir )
        {
            ErrMsg( "scratch-path '%s' is not a directory!", tool_ctx -> temp_path );
            rc = RC( rcExe, rcNoTarg, rcConstructing, rcPath, rcInvalid );
        }
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
        tool_ctx tool_ctx;
        rc = ArgsParamValue( args, 0, ( const void ** )&( tool_ctx . cmn . acc ) );
        if ( rc != 0 )
            ErrMsg( "ArgsParamValue() -> %R", rc );
        else
        {
            tool_ctx . cmn . row_range = get_str_option( args, OPTION_RANGE, NULL );
            tool_ctx . cmn . cursor_cache = get_size_t_option( args, OPTION_CURCACHE, 5 * 1024 * 1024 );            
            tool_ctx . cmn . show_progress = get_bool_option( args, OPTION_PROGRESS );
			tool_ctx . cmn . show_details = get_bool_option( args, OPTION_DETAILS );
            tool_ctx . cmn . count = 0;

           
            tool_ctx . temp_path = get_str_option( args, OPTION_TEMP, NULL );
            tool_ctx . remove_temp_path = false;
            tool_ctx . output_filename = get_str_option( args, OPTION_OUTPUT, NULL );
            tool_ctx . lookup_filename = get_str_option( args, OPTION_LOOKUP, NULL );            
            tool_ctx . index_filename = get_str_option( args, OPTION_INDEX, NULL );
            tool_ctx . buf_size = get_size_t_option( args, OPTION_BUFSIZE, 1024 * 1024 );
            tool_ctx . mem_limit = get_size_t_option( args, OPTION_MEM, 1024L * 1024 * 100 );
            tool_ctx . num_threads = get_uint64_t_option( args, OPTION_THREADS, 1 );
            tool_ctx . fmt = get_format_t( get_str_option( args, OPTION_FORMAT, NULL ) ); /* helper.c */
            
            if ( get_bool_option( args, OPTION_SPLIT ) && tool_ctx . fmt == ft_fastq )
                tool_ctx . fmt = ft_fastq_split;
                
            rc = KDirectoryNativeDir( &( tool_ctx . cmn . dir ) );
            if ( rc != 0 )
                ErrMsg( "KDirectoryNativeDir() -> %R", rc );
            else
            {
                rc = check_temp_path( &tool_ctx );
                if ( rc == 0 && tool_ctx.cmn.show_details )
                    rc = show_details( &tool_ctx );
                if ( rc == 0  )
                {
                    char dflt_lookup[ 4096 ];
                    char dflt_index[ 4096 ];
                    char dflt_output[ 4096 ];
                
                    dflt_lookup[ 0 ] = 0;
                    dflt_index[ 0 ] = 0;
                    dflt_output[ 0 ] = 0;
                
                    if ( tool_ctx . lookup_filename == NULL )
                    {
                        rc = make_prefixed( dflt_lookup, sizeof dflt_lookup, tool_ctx . temp_path,
                                            tool_ctx . cmn . acc, ".lookup" ); /* helper.c */
                        if ( rc == 0 )
                            tool_ctx . lookup_filename = dflt_lookup;
                    }

                    if ( tool_ctx . index_filename == NULL )
                    {
                        rc = make_prefixed( dflt_index, sizeof dflt_index, tool_ctx . temp_path,
                                            tool_ctx . cmn . acc, ".lookup.idx" ); /* helper.c */
                        if ( rc == 0 )
                            tool_ctx . index_filename = dflt_index;
                    }

                    if ( tool_ctx . output_filename == NULL )
                    {
                        rc = make_prefixed( dflt_output, sizeof dflt_output, NULL,
                                            tool_ctx . cmn . acc, ".txt" ); /* helper.c */
                        if ( rc == 0 )
                            tool_ctx . output_filename = dflt_output;
                    }

                    rc = perform_join( &tool_ctx ); /* <==== above! */

                    if ( dflt_lookup[ 0 ] != 0 )
                        KDirectoryRemove( tool_ctx . cmn . dir, true, "%s", dflt_lookup );

                    if ( dflt_index[ 0 ] != 0 )
                        KDirectoryRemove( tool_ctx . cmn . dir, true, "%s", dflt_index );
                        
                    if ( tool_ctx . remove_temp_path )
                    {
                        rc_t rc1 = KDirectoryClearDir ( tool_ctx . cmn . dir, true, "%s", tool_ctx . temp_path );
                        if ( rc1 == 0 )
                            rc1 = KDirectoryRemove ( tool_ctx . cmn . dir, true, "%s", tool_ctx . temp_path );
                    }
                }
                KDirectoryRelease( tool_ctx . cmn . dir );
            }
        }
    }
    return rc;
}
