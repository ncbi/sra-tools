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
#include "sorter.h"
#include "merge_sorter.h"
#include "join.h"
#include "tbl_join.h"
#include "concatenator.h"
#include "cleanup_task.h"
#include "lookup_reader.h"
#include "raw_read_iter.h"
#include "temp_dir.h"

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/out.h>
#include <klib/printf.h>
#include <search/grep.h>
#include <kfs/directory.h>
#include <kproc/procmgr.h>

#include <stdio.h>
#include <os-native.h>
#include <sysalloc.h>

static const char * format_usage[] = { "format (special, fastq, lookup, default=special)", NULL };
#define OPTION_FORMAT   "format"
#define ALIAS_FORMAT    "F"

static const char * outputf_usage[] = { "output-file", NULL };
#define OPTION_OUTPUT_F "outfile"
#define ALIAS_OUTPUT_F  "o"

static const char * outputd_usage[] = { "output-dir", NULL };
#define OPTION_OUTPUT_D "outdir"
#define ALIAS_OUTPUT_D  "O"

static const char * progress_usage[] = { "show progress", NULL };
#define OPTION_PROGRESS "progress"
#define ALIAS_PROGRESS  "p"

static const char * bufsize_usage[] = { "size of file-buffer dflt=1MB", NULL };
#define OPTION_BUFSIZE  "bufsize"
#define ALIAS_BUFSIZE   "b"

static const char * curcache_usage[] = { "size of cursor-cache dflt=10MB", NULL };
#define OPTION_CURCACHE "curcache"
#define ALIAS_CURCACHE  "c"

static const char * mem_usage[] = { "memory limit for sorting dflt=100MB", NULL };
#define OPTION_MEM      "mem"
#define ALIAS_MEM       "m"

static const char * temp_usage[] = { "where to put temp. files dflt=curr dir", NULL };
#define OPTION_TEMP     "temp"
#define ALIAS_TEMP      "t"

static const char * threads_usage[] = { "how many thread dflt=6", NULL };
#define OPTION_THREADS  "threads"
#define ALIAS_THREADS   "e"

static const char * detail_usage[] = { "print details", NULL };
#define OPTION_DETAILS  "details"
#define ALIAS_DETAILS    "x"

static const char * split_spot_usage[] = { "split spots into reads", NULL };
#define OPTION_SPLIT_SPOT "split-spot"
#define ALIAS_SPLIT_SPOT  "s"

static const char * split_file_usage[] = { "write reads into different files", NULL };
#define OPTION_SPLIT_FILE "split-files"
#define ALIAS_SPLIT_FILE  "S"

static const char * split_3_usage[] = { "writes single reads in special file", NULL };
#define OPTION_SPLIT_3   "split-3"
#define ALIAS_SPLIT_3    "3"

static const char * whole_spot_usage[] = { "writes whole spots into one file", NULL };
#define OPTION_WHOLE_SPOT   "concatenate-reads"

/*
static const char * stdout_usage[] = { "print output to stdout", NULL };
#define OPTION_STDOUT    "stdout"
#define ALIAS_STDOUT     "Z"

static const char * gzip_usage[] = { "compress output using gzip", NULL };
#define OPTION_GZIP      "gzip"
#define ALIAS_GZIP       "g"

static const char * bzip2_usage[] = { "compress output using bzip2", NULL };
#define OPTION_BZIP2     "bzip2"
#define ALIAS_BZIP2      "z"

static const char * maxfd_usage[] = { "maximal number of file-descriptors", NULL };
#define OPTION_MAXFD     "maxfd"
#define ALIAS_MAXFD      "a"
*/

static const char * force_usage[] = { "force to overwrite existing file(s)", NULL };
#define OPTION_FORCE     "force"
#define ALIAS_FORCE      "f"

static const char * ridn_usage[] = { "use row-id as name", NULL };
#define OPTION_RIDN      "rowid-as-name"
#define ALIAS_RIDN       "N"

static const char * skip_tech_usage[] = { "skip technical reads", NULL };
#define OPTION_SKIP_TECH      "skip-technical"

static const char * incl_tech_usage[] = { "include technical reads", NULL };
#define OPTION_INCL_TECH      "include-technical"

static const char * print_read_nr[] = { "print read-numbers", NULL };
#define OPTION_PRNR      "print-read-nr"
#define ALIAS_PRNR       "P"

static const char * min_rl_usage[] = { "filter by sequence-len", NULL };
#define OPTION_MINRDLEN  "min-read-len"
#define ALIAS_MINRDLEN   "M"

static const char * base_flt_usage[] = { "filter by bases", NULL };
#define OPTION_BASE_FLT  "bases"
#define ALIAS_BASE_FLT   "B"

static const char * table_usage[] = { "which seq-table to use in case of pacbio", NULL };
#define OPTION_TABLE    "table"

static const char * strict_usage[] = { "terminate on invalid read", NULL };
#define OPTION_STRICT   "strict"

static const char * append_usage[] = { "append to output-file", NULL };
#define OPTION_APPEND   "append"
#define ALIAS_APPEND    "A"

OptDef ToolOptions[] =
{
    { OPTION_FORMAT,    ALIAS_FORMAT,    NULL, format_usage,     1, true,   false },
    { OPTION_OUTPUT_F,  ALIAS_OUTPUT_F,  NULL, outputf_usage,    1, true,   false },
    { OPTION_OUTPUT_D,  ALIAS_OUTPUT_D,  NULL, outputd_usage,    1, true,   false },
    { OPTION_BUFSIZE,   ALIAS_BUFSIZE,   NULL, bufsize_usage,    1, true,   false },
    { OPTION_CURCACHE,  ALIAS_CURCACHE,  NULL, curcache_usage,   1, true,   false },
    { OPTION_MEM,       ALIAS_MEM,       NULL, mem_usage,        1, true,   false },
    { OPTION_TEMP,      ALIAS_TEMP,      NULL, temp_usage,       1, true,   false },
    { OPTION_THREADS,   ALIAS_THREADS,   NULL, threads_usage,    1, true,   false },
    { OPTION_PROGRESS,  ALIAS_PROGRESS,  NULL, progress_usage,   1, false,  false },
    { OPTION_DETAILS,   ALIAS_DETAILS,   NULL, detail_usage,     1, false,  false },
    { OPTION_SPLIT_SPOT,ALIAS_SPLIT_SPOT,NULL, split_spot_usage, 1, false,  false },
    { OPTION_SPLIT_FILE,ALIAS_SPLIT_FILE,NULL, split_file_usage, 1, false,  false },
    { OPTION_SPLIT_3,   ALIAS_SPLIT_3,   NULL, split_3_usage,    1, false,  false },
    { OPTION_WHOLE_SPOT,    NULL,        NULL, whole_spot_usage, 1, false,  false },    
/*    { OPTION_STDOUT,    ALIAS_STDOUT,    NULL, stdout_usage,     1, false,  false }, */
/*    { OPTION_GZIP,      ALIAS_GZIP,      NULL, gzip_usage,       1, false,  false }, */
/*    { OPTION_BZIP2,     ALIAS_BZIP2,     NULL, bzip2_usage,      1, false,  false }, */
/*    { OPTION_MAXFD,     ALIAS_MAXFD,     NULL, maxfd_usage,      1, true,   false }, */
    { OPTION_FORCE,     ALIAS_FORCE,     NULL, force_usage,      1, false,  false },
    { OPTION_RIDN,      ALIAS_RIDN,      NULL, ridn_usage,       1, false,  false },
    { OPTION_SKIP_TECH, NULL,            NULL, skip_tech_usage,  1, false,  false },
    { OPTION_INCL_TECH, NULL,            NULL, incl_tech_usage,  1, false,  false },
    { OPTION_PRNR,      ALIAS_PRNR,      NULL, print_read_nr,    1, false,  false },
    { OPTION_MINRDLEN,  ALIAS_MINRDLEN,  NULL, min_rl_usage,     1, true,   false },
    { OPTION_TABLE,     NULL,            NULL, table_usage,      1, true,   false },
    { OPTION_STRICT,    NULL,            NULL, strict_usage,     1, false,  false },
    { OPTION_BASE_FLT,  ALIAS_BASE_FLT,  NULL, base_flt_usage,   10, true,  false },
    { OPTION_APPEND,    ALIAS_APPEND,    NULL, append_usage,     1, false,  false }    
};

const char UsageDefaultName[] = "fasterq-dump";

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

#define DFLT_PATH_LEN 4096

typedef struct tool_ctx_t
{
    KDirectory * dir;

    const char * requested_temp_path;
    const char * accession_path;
    const char * accession_short;
    const char * output_filename;
    const char * output_dirname;
    const char * seq_tbl_name;
    
    struct temp_dir * temp_dir; /* temp_dir.h */
    
    char lookup_filename[ DFLT_PATH_LEN ];
    char index_filename[ DFLT_PATH_LEN ];
    char dflt_output[ DFLT_PATH_LEN ];
    
    struct KFastDumpCleanupTask * cleanup_task; /* cleanup_task.h */
    
    size_t cursor_cache, buf_size, mem_limit;

    uint32_t num_threads /*, max_fds */;
    uint64_t total_ram;
    
    format_t fmt; /* helper.h */

    compress_t compress; /* helper.h */ 

    bool force, show_progress, show_details, append;
    
    join_options join_options; /* helper.h */
} tool_ctx_t;

/* taken form libs/kapp/main-priv.h */
rc_t KAppGetTotalRam ( uint64_t * totalRam );

static rc_t get_environment( tool_ctx_t * tool_ctx )
{
    rc_t rc = KAppGetTotalRam ( &( tool_ctx -> total_ram ) );
    if ( rc != 0 )
        ErrMsg( "KAppGetTotalRam() -> %R", rc );
    if ( rc == 0 )
    {
        rc = KDirectoryNativeDir( &( tool_ctx -> dir ) );
        if ( rc != 0 )
            ErrMsg( "KDirectoryNativeDir() -> %R", rc );
    }
    return rc;
}

static rc_t show_details( tool_ctx_t * tool_ctx )
{
    rc_t rc = KOutMsg( "cursor-cache : %,ld bytes\n", tool_ctx -> cursor_cache );
    if ( rc == 0 )
        rc = KOutMsg( "buf-size     : %,ld bytes\n", tool_ctx -> buf_size );
    if ( rc == 0 )
        rc = KOutMsg( "mem-limit    : %,ld bytes\n", tool_ctx -> mem_limit );
    if ( rc == 0 )
        rc = KOutMsg( "threads      : %d\n", tool_ctx -> num_threads );
    if ( rc == 0 )
        rc = KOutMsg( "scratch-path : '%s'\n", get_temp_dir( tool_ctx -> temp_dir ) );
    if ( rc == 0 )
        rc = KOutMsg( "output-format: " );
    if ( rc == 0 )
    {
        switch ( tool_ctx -> fmt )
        {
            case ft_special             : rc = KOutMsg( "SPECIAL\n" ); break;
            case ft_whole_spot          : rc = KOutMsg( "FASTQ whole spot\n" ); break;
            case ft_fastq_split_spot    : rc = KOutMsg( "FASTQ split spot\n" ); break;
            case ft_fastq_split_file    : rc = KOutMsg( "FASTQ split file\n" ); break;
            case ft_fastq_split_3       : rc = KOutMsg( "FASTQ split 3\n" ); break;
            default                     : rc = KOutMsg( "unknow format\n" ); break;
        }
    }
    if ( rc == 0 )
    {
        rc = KOutMsg( "output-file  : '%s'\n", tool_ctx -> output_filename );
        rc = KOutMsg( "output-dir   : '%s'\n", tool_ctx -> output_dirname );
    }
    if ( rc == 0 )
    {
        rc = KOutMsg( "append-mode  : '%s'\n", tool_ctx -> append ? "YES" : "NO" );
    }
    return rc;
}

static const char * dflt_seq_tabl_name = "SEQUENCE";

#define DFLT_CUR_CACHE ( 5 * 1024 * 1024 )
#define DFLT_BUF_SIZE ( 1024 * 1024 )
#define DFLT_MEM_LIMIT ( 1024L * 1024 * 50 )
#define DFLT_NUM_THREADS 6
static void get_user_input( tool_ctx_t * tool_ctx, const Args * args )
{
    bool split_spot, split_file, split_3, whole_spot;
    
    /*
    tool_ctx -> compress = get_compress_t( get_bool_option( args, OPTION_GZIP ),
                                            get_bool_option( args, OPTION_BZIP2 ) ); helper.c */
    tool_ctx -> compress = ct_none;
    
    tool_ctx -> cursor_cache = get_size_t_option( args, OPTION_CURCACHE, DFLT_CUR_CACHE );            
    tool_ctx -> show_progress = get_bool_option( args, OPTION_PROGRESS );
    tool_ctx -> show_details = get_bool_option( args, OPTION_DETAILS );
    tool_ctx -> requested_temp_path = get_str_option( args, OPTION_TEMP, NULL );
    tool_ctx -> force = get_bool_option( args, OPTION_FORCE );        
    tool_ctx -> output_filename = get_str_option( args, OPTION_OUTPUT_F, NULL );
    tool_ctx -> output_dirname = get_str_option( args, OPTION_OUTPUT_D, NULL );
    tool_ctx -> buf_size = get_size_t_option( args, OPTION_BUFSIZE, DFLT_BUF_SIZE );
    tool_ctx -> mem_limit = get_size_t_option( args, OPTION_MEM, DFLT_MEM_LIMIT );
    tool_ctx -> num_threads = get_uint32_t_option( args, OPTION_THREADS, DFLT_NUM_THREADS );

    tool_ctx -> join_options . rowid_as_name = get_bool_option( args, OPTION_RIDN );
    tool_ctx -> join_options . skip_tech = !( get_bool_option( args, OPTION_INCL_TECH ) );
    tool_ctx -> join_options . print_read_nr = get_bool_option( args, OPTION_PRNR );
    tool_ctx -> join_options . print_name = true;
    tool_ctx -> join_options . min_read_len = get_uint32_t_option( args, OPTION_MINRDLEN, 0 );
    tool_ctx -> join_options . filter_bases = get_str_option( args, OPTION_BASE_FLT, NULL );
    tool_ctx -> join_options . terminate_on_invalid = get_bool_option( args, OPTION_STRICT );

    split_spot = get_bool_option( args, OPTION_SPLIT_SPOT );
    split_file = get_bool_option( args, OPTION_SPLIT_FILE );
    split_3    = get_bool_option( args, OPTION_SPLIT_3 );
    whole_spot = get_bool_option( args, OPTION_WHOLE_SPOT );
    
    tool_ctx -> fmt = get_format_t( get_str_option( args, OPTION_FORMAT, NULL ),
                            split_spot, split_file, split_3, whole_spot ); /* helper.c */
    if ( tool_ctx -> fmt == ft_fastq_split_3 )
        tool_ctx -> join_options . skip_tech = true;

    tool_ctx -> seq_tbl_name = get_str_option( args, OPTION_TABLE, dflt_seq_tabl_name );
    tool_ctx -> append = get_bool_option( args, OPTION_APPEND );
}

#define DFLT_MAX_FD 32
#define MIN_NUM_THREADS 2
#define MIN_MEM_LIMIT ( 1024L * 1024 * 5 )
#define MAX_BUF_SIZE ( 1024L * 1024 * 1024 )
static void encforce_constrains( tool_ctx_t * tool_ctx )
{
    if ( tool_ctx -> num_threads < MIN_NUM_THREADS )
        tool_ctx -> num_threads = MIN_NUM_THREADS;
        
    if ( tool_ctx -> mem_limit < MIN_MEM_LIMIT )
        tool_ctx -> mem_limit = MIN_MEM_LIMIT;

    if ( tool_ctx -> buf_size > MAX_BUF_SIZE )
        tool_ctx -> buf_size = MAX_BUF_SIZE;
}


static rc_t handle_accession( tool_ctx_t * tool_ctx )
{
    rc_t rc = 0;
    tool_ctx -> accession_short = extract_acc( tool_ctx -> accession_path );
    if ( tool_ctx -> accession_short == NULL )
    {
        rc = RC( rcApp, rcArgv, rcAccessing, rcParam, rcInvalid );            
        ErrMsg( "accession '%s' invalid", tool_ctx -> accession_path );
    }
    return rc;
}

static rc_t handle_lookup_path( tool_ctx_t * tool_ctx )
{
    rc_t rc = generate_lookup_filename( tool_ctx -> temp_dir,
                                        &tool_ctx -> lookup_filename[ 0 ],
                                        sizeof tool_ctx -> lookup_filename );
    if ( rc != 0 )
        ErrMsg( "fasterq-dump.c handle_lookup_path( lookup_filename ) -> %R", rc );
    else
    {
        size_t num_writ;
        /* generate the full path of the lookup-index-table */                
        rc = string_printf( &tool_ctx -> index_filename[ 0 ], sizeof tool_ctx -> index_filename,
                            &num_writ,
                            "%s.idx",
                            &tool_ctx -> lookup_filename[ 0 ] );
        if ( rc != 0 )
            ErrMsg( "fasterq-dump.c handle_lookup_path( index_filename ) -> %R", rc );
    }
    return rc;
}

/* we have NO output-dir and NO output-file */
static rc_t make_output_filename_from_accession( tool_ctx_t * tool_ctx )
{
    /* we DO NOT have a output-directory : build output-filename from the accession */
    /* generate the full path of the output-file, if not given */
    size_t num_writ;        
    rc_t rc = string_printf( &tool_ctx -> dflt_output[ 0 ], sizeof tool_ctx -> dflt_output,
                        &num_writ,
                        "%s.fastq",
                        tool_ctx -> accession_short );
    if ( rc != 0 )
        ErrMsg( "string_printf( output-filename ) -> %R", rc );
    else
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
    return rc;
}

/* we have an output-dir and NO output-file */
static rc_t make_output_filename_from_dir_and_accession( tool_ctx_t * tool_ctx )
{
    size_t num_writ;
    bool es = ends_in_slash( tool_ctx -> output_dirname ); /* helper.c */
    rc_t rc = string_printf( tool_ctx -> dflt_output, sizeof tool_ctx -> dflt_output,
                        &num_writ,
                        es ? "%s%s.fastq" : "%s/%s.fastq",
                        tool_ctx -> output_dirname,
                        tool_ctx -> accession_short );
    if ( rc != 0 )
        ErrMsg( "string_printf( output-filename ) -> %R", rc );
    else
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
    return rc;
}

static rc_t optionally_create_paths_in_output_filename( tool_ctx_t * tool_ctx )
{
    rc_t rc = 0;
    String path;
    if ( extract_path( tool_ctx -> output_filename, &path ) )
    {
        /* the output-filename contains a path... */
        if ( !dir_exists( tool_ctx -> dir, "%S", &path ) )
        {
            /* this path does not ( yet ) exist, create it... */
            rc = create_this_dir( tool_ctx -> dir, &path, true );
        }
    }
    return rc;
}

static rc_t adjust_output_filename( tool_ctx_t * tool_ctx )
{
    rc_t rc = 0;
    /* we do have a output-filename : use it */
    if ( dir_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename ) ) /* helper.c */
    {
        /* the given output-filename is an existing directory ! */
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "string_printf( output-filename ) -> %R", rc );
    }
    else
        rc = optionally_create_paths_in_output_filename( tool_ctx );
    return rc;
}

static rc_t adjust_output_filename_by_dir( tool_ctx_t * tool_ctx )
{
    size_t num_writ;
    bool es = ends_in_slash( tool_ctx -> output_dirname ); /* helper.c */
    rc_t rc = string_printf( tool_ctx -> dflt_output, sizeof tool_ctx -> dflt_output,
                        &num_writ,
                        es ? "%s%s" : "%s/%s",
                        tool_ctx -> output_dirname,
                        tool_ctx -> output_filename );
    if ( rc != 0 )
        ErrMsg( "string_printf( output-filename ) -> %R", rc );
    else
    {
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
        rc = optionally_create_paths_in_output_filename( tool_ctx );
    }
    return rc;
}

static rc_t populate_tool_ctx( tool_ctx_t * tool_ctx, const Args * args )
{
    rc_t rc = ArgsParamValue( args, 0, ( const void ** )&( tool_ctx -> accession_path ) );
    if ( rc != 0 )
        ErrMsg( "ArgsParamValue() -> %R", rc );
    else
    {
        tool_ctx -> lookup_filename[ 0 ] = 0;
        tool_ctx -> index_filename[ 0 ] = 0;
        tool_ctx -> dflt_output[ 0 ] = 0;
    
        get_user_input( tool_ctx, args );
        encforce_constrains( tool_ctx );
        get_environment( tool_ctx );
        
        rc = make_temp_dir( &tool_ctx -> temp_dir,
                          tool_ctx -> requested_temp_path,
                          tool_ctx -> dir );
    }
    
    if ( rc == 0 )
        rc = handle_accession( tool_ctx );
    
    if ( rc == 0 )
        rc = handle_lookup_path( tool_ctx );
    
    if ( rc == 0 && tool_ctx -> output_dirname != NULL )
    {
        if ( !dir_exists( tool_ctx -> dir, "%s", tool_ctx -> output_dirname ) )
            rc = create_this_dir_2( tool_ctx -> dir, tool_ctx -> output_dirname, true );
    }
    
    if ( rc == 0 )
    {
        if ( tool_ctx -> output_filename == NULL )
        {
            if ( tool_ctx -> output_dirname == NULL )
                rc = make_output_filename_from_accession( tool_ctx );
            else
                rc = make_output_filename_from_dir_and_accession( tool_ctx );
        }
        else
        {
            if ( tool_ctx -> output_dirname == NULL )
                rc = adjust_output_filename( tool_ctx );
            else
                rc = adjust_output_filename_by_dir( tool_ctx );
        }
    }
    
    if ( rc == 0 )
        rc = Make_FastDump_Cleanup_Task ( &( tool_ctx -> cleanup_task ) ); /* cleanup_task.c */
    if ( rc == 0 )
        rc = Add_Directory_to_Cleanup_Task ( tool_ctx -> cleanup_task, 
                get_temp_dir( tool_ctx -> temp_dir ) );
    return rc;
}

static rc_t print_stats( const join_stats * stats )
{
    rc_t rc = KOutMsg( "spots read      : %,lu\n", stats -> spots_read );
    if ( rc == 0 )
         rc = KOutMsg( "reads read      : %,lu\n", stats -> reads_read );
    if ( rc == 0 )
         rc = KOutMsg( "reads written   : %,lu\n", stats -> reads_written );
    if ( rc == 0 && stats -> reads_zero_length > 0 )
         rc = KOutMsg( "reads 0-length  : %,lu\n", stats -> reads_zero_length );
    if ( rc == 0 && stats -> reads_technical > 0 )
         rc = KOutMsg( "technical reads : %,lu\n", stats -> reads_technical );
    if ( rc == 0 && stats -> reads_too_short > 0 )
         rc = KOutMsg( "reads too short : %,lu\n", stats -> reads_too_short );
    if ( rc == 0 && stats -> reads_invalid > 0 )
         rc = KOutMsg( "reads invalid   : %,lu\n", stats -> reads_invalid );
    return rc;
}

/* --------------------------------------------------------------------------------------------
    produce special-output ( SPOT_ID,READ,SPOT_GROUP ) by iterating over the SEQUENCE - table:
    produce fastq-output by iterating over the SEQUENCE - table:
   -------------------------------------------------------------------------------------------- 
   each thread iterates over a slice of the SEQUENCE-table
   for each SPOT it may look up an entry in the lookup-table to get the READ
   if it is not stored in the SEQ-tbl
-------------------------------------------------------------------------------------------- */

static const uint32_t queue_timeout = 200;  /* ms */

static rc_t produce_lookup_files( tool_ctx_t * tool_ctx )
{
    rc_t rc = 0;
    struct bg_update * gap = NULL;
    struct background_file_merger * bg_file_merger;
    struct background_vector_merger * bg_vec_merger;
    
    if ( tool_ctx -> show_progress )
        rc = bg_update_make( &gap, 0 );
        
    /* the background-file-merger catches the files produced by
        the background-vector-merger */
    if ( rc == 0 )            
        rc = make_background_file_merger( &bg_file_merger,
                                tool_ctx -> dir,
                                tool_ctx -> temp_dir,
                                tool_ctx -> cleanup_task,
                                &tool_ctx -> lookup_filename[ 0 ],
                                &tool_ctx -> index_filename[ 0 ],
                                tool_ctx -> num_threads,
                                queue_timeout,
                                tool_ctx -> buf_size,
                                gap ); /* merge_sorter.c */

    /* the background-vector-merger catches the KVectors produced by
       the lookup-produceer */
    if ( rc == 0 )
        rc = make_background_vector_merger( &bg_vec_merger,
                 tool_ctx -> dir,
                 tool_ctx -> temp_dir,
                 tool_ctx -> cleanup_task,
                 bg_file_merger,
                 tool_ctx -> num_threads,
                 queue_timeout,
                 tool_ctx -> buf_size,
                 gap ); /* merge_sorter.c */
        
/* --------------------------------------------------------------------------------------------
    produce the lookup-table by iterating over the PRIMARY_ALIGNMENT - table:
   -------------------------------------------------------------------------------------------- 
    reading SEQ_SPOT_ID, SEQ_READ_ID and RAW_READ
    SEQ_SPOT_ID and SEQ_READ_ID is merged into a 64-bit-key
    RAW_READ is read as 4na-unpacked ( Schema does not provide 4na-packed for this column )
    these key-pairs are temporarely stored in a KVector until a limit is reached
    after that limit is reached they are pushed to the background-vector-merger
    This KVector looks like this:
    content: [KEY][RAW_READ]
    KEY... 64-bit value as SEQ_SPOT_ID shifted left by 1 bit, zero-bit contains SEQ_READ_ID
    RAW_READ... 16-bit binary-chunk-lenght, followed by n bytes of packed 4na
-------------------------------------------------------------------------------------------- */
    /* the lookup-producer is the source of the chain */
    if ( rc == 0 )
        rc = execute_lookup_production( tool_ctx -> dir,
                                        tool_ctx -> accession_short,
                                        bg_vec_merger, /* drives the bg_file_merger */
                                        tool_ctx -> cursor_cache,
                                        tool_ctx -> buf_size,
                                        tool_ctx -> mem_limit,
                                        tool_ctx -> num_threads,
                                        tool_ctx -> show_progress ); /* sorter.c */

    bg_update_start( gap, "merge  : " ); /* progress_thread.c ...start showing the activity... */
            
    if ( rc == 0 )
        rc = wait_for_and_release_background_vector_merger( bg_vec_merger ); /* merge_sorter.c */
            
    if ( rc == 0 )
        rc = wait_for_and_release_background_file_merger( bg_file_merger ); /* merge_sorter.c */

    bg_update_release( gap );

    if ( rc != 0 )
        ErrMsg( "fasterq-dump.c produce_lookup_files() -> %R", rc );
        
    return rc;
}


/* -------------------------------------------------------------------------------------------- */


static rc_t produce_final_db_output( tool_ctx_t * tool_ctx )
{
    struct temp_registry * registry = NULL;
    join_stats stats;
    
    rc_t rc = make_temp_registry( &registry, tool_ctx -> cleanup_task ); /* temp_registry.c */
    
    clear_join_stats( &stats ); /* helper.c */
    /* join SEQUENCE-table with lookup-table === this is the actual purpos of the tool === */
    
/* --------------------------------------------------------------------------------------------
    produce special-output ( SPOT_ID,READ,SPOT_GROUP ) by iterating over the SEQUENCE - table:
    produce fastq-output by iterating over the SEQUENCE - table:
   -------------------------------------------------------------------------------------------- 
   each thread iterates over a slice of the SEQUENCE-table
   for each SPOT it may look up an entry in the lookup-table to get the READ
   if it is not stored in the SEQ-tbl
-------------------------------------------------------------------------------------------- */
    
    if ( rc == 0 )
        rc = execute_db_join( tool_ctx -> dir,
                           tool_ctx -> accession_path,
                           tool_ctx -> accession_short,
                           &stats,
                           &tool_ctx -> lookup_filename[ 0 ],
                           &tool_ctx -> index_filename[ 0 ],
                           tool_ctx -> temp_dir,
                           registry,
                           tool_ctx -> cursor_cache,
                           tool_ctx -> buf_size,
                           tool_ctx -> num_threads,
                           tool_ctx -> show_progress,
                           tool_ctx -> fmt,
                           & tool_ctx -> join_options ); /* join.c */

    /* from now on we do not need the lookup-file and it's index any more... */
    if ( tool_ctx -> lookup_filename[ 0 ] != 0 )
        KDirectoryRemove( tool_ctx -> dir, true, "%s", &tool_ctx -> lookup_filename[ 0 ] );

    if ( tool_ctx -> index_filename[ 0 ] != 0 )
        KDirectoryRemove( tool_ctx -> dir, true, "%s", &tool_ctx -> index_filename[ 0 ] );

    /* STEP 4 : concatenate output-chunks */
    if ( rc == 0 )
        rc = temp_registry_merge( registry,
                          tool_ctx -> dir,
                          tool_ctx -> output_filename,
                          tool_ctx -> buf_size,
                          tool_ctx -> show_progress,
                          tool_ctx -> force,
                          tool_ctx -> compress,
                          tool_ctx -> append ); /* temp_registry.c */

    /* in case some of the partial results have not been deleted be the concatenator */
    if ( registry != NULL )
        destroy_temp_registry( registry ); /* temp_registry.c */

    if ( rc == 0 )
        print_stats( &stats ); /* helper.c */

    return rc;
}

/* -------------------------------------------------------------------------------------------- */

static bool output_exists_whole( tool_ctx_t * tool_ctx )
{
    return file_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename );
}

static bool output_exists_idx( tool_ctx_t * tool_ctx, uint32_t idx )
{
    bool res = false;
    SBuffer s_filename;
    rc_t rc = split_filename_insert_idx( &s_filename, 4096,
                            tool_ctx -> output_filename, idx ); /* helper.c */
    if ( rc == 0 )
    {
        res = file_exists( tool_ctx -> dir, "%S", &( s_filename . S ) ); /* helper.c */
        release_SBuffer( &s_filename ); /* helper.c */
    }
    return res;
}

static bool output_exists_split( tool_ctx_t * tool_ctx )
{
    bool res = output_exists_whole( tool_ctx );
    if ( !res ) res = output_exists_idx( tool_ctx, 1 );
    if ( !res ) res = output_exists_idx( tool_ctx, 2 );    
    return res;
}

static rc_t check_output_exits( tool_ctx_t * tool_ctx )
{
    rc_t rc = 0;
    /* check if the output-file(s) do already exist, in case we are not overwriting */    
    if ( !( tool_ctx -> force ) )
    {
        bool exists = false;
        switch( tool_ctx -> fmt )
        {
            case ft_unknown             : break;
            case ft_special             : exists = output_exists_whole( tool_ctx ); break;
            case ft_whole_spot          : exists = output_exists_whole( tool_ctx ); break;
            case ft_fastq_split_spot    : exists = output_exists_whole( tool_ctx ); break;
            case ft_fastq_split_file    : exists = output_exists_split( tool_ctx ); break;
            case ft_fastq_split_3       : exists = output_exists_split( tool_ctx ); break;
        }
        if ( exists )
        {
            rc = RC( rcExe, rcFile, rcPacking, rcName, rcExists );
            ErrMsg( "fasterq-dump.c fastdump_csra() checking ouput-file '%s' -> %R",
                     tool_ctx -> output_filename, rc );
        }
    }
    return rc;
}

static rc_t fastdump_csra( tool_ctx_t * tool_ctx )
{
    rc_t rc = 0;
    
    if ( tool_ctx -> show_details )
        rc = show_details( tool_ctx ); /* above */

    if ( rc == 0 )
        rc = check_output_exits( tool_ctx ); /* above */
    
    if ( rc == 0 )
        rc = produce_lookup_files( tool_ctx ); /* above */

    if ( rc == 0 )
        rc = produce_final_db_output( tool_ctx ); /* above */

    return rc;
}


/* -------------------------------------------------------------------------------------------- */

static rc_t fastdump_table( tool_ctx_t * tool_ctx, const char * tbl_name )
{
    rc_t rc = 0;
    struct temp_registry * registry = NULL;
    join_stats stats;
    
    clear_join_stats( &stats ); /* helper.c */
    
    if ( tool_ctx -> show_details )
        rc = show_details( tool_ctx ); /* above */

    if ( rc == 0 )
        rc = check_output_exits( tool_ctx ); /* above */

    if ( rc == 0 )
        rc = make_temp_registry( &registry, tool_ctx -> cleanup_task ); /* temp_registry.c */

    if ( rc == 0 )
        rc = execute_tbl_join( tool_ctx -> dir,
                           tool_ctx -> accession_path,
                           tool_ctx -> accession_short,
                           &stats,
                           tbl_name,
                           tool_ctx -> temp_dir,
                           registry,
                           tool_ctx -> cursor_cache,
                           tool_ctx -> buf_size,
                           tool_ctx -> num_threads,
                           tool_ctx -> show_progress,
                           tool_ctx -> fmt,
                           & tool_ctx -> join_options ); /* tbl_join.c */

    if ( rc == 0 )
        rc = temp_registry_merge( registry,
                          tool_ctx -> dir,
                          tool_ctx -> output_filename,
                          tool_ctx -> buf_size,
                          tool_ctx -> show_progress,
                          tool_ctx -> force,
                          tool_ctx -> compress,
                          tool_ctx -> append ); /* temp_registry.c */

    if ( registry != NULL )
        destroy_temp_registry( registry ); /* temp_registry.c */

    if ( rc == 0 )
        print_stats( &stats ); /* helper.c */

    return rc;
}

static const char * consensus_table = "CONSENSUS";

static const char * get_db_seq_tbl_name( tool_ctx_t * tool_ctx )
{
    const char * res = tool_ctx -> seq_tbl_name;
    VNamelist * tables = cmn_get_table_names( tool_ctx -> dir, tool_ctx -> accession_path ); /* cmn_iter.c */
    if ( tables != NULL )
    {
        int32_t idx;
        rc_t rc = VNamelistContainsStr( tables, consensus_table, &idx );
        if ( rc == 0 && idx > -1 )
            res = consensus_table;
        VNamelistRelease ( tables );
    }
    return res;
}

/* -------------------------------------------------------------------------------------------- */

static rc_t perform_tool( tool_ctx_t * tool_ctx )
{
    acc_type_t acc_type;
    rc_t rc = cmn_get_acc_type( tool_ctx -> dir, tool_ctx -> accession_path, &acc_type ); /* cmn_iter.c */
    if ( rc == 0 )
    {
        /* =================================================== */
        switch( acc_type )
        {
            case acc_csra       : rc = fastdump_csra( tool_ctx ); break; /* above */
            case acc_pacbio     : ErrMsg( "accession '%s' is PACBIO, please use fastq-dump instead", tool_ctx -> accession_path ); break;
            case acc_sra_flat   : rc = fastdump_table( tool_ctx, NULL ); break; /* above */
            case acc_sra_db     : rc = fastdump_table( tool_ctx, get_db_seq_tbl_name( tool_ctx ) ); break; /* above */
            default : ErrMsg( "invalid accession '%s'", tool_ctx -> accession_path );
        }
        /* =================================================== */
        
        /* remove_temp_dir( tool_ctx -> temp_dir, tool_ctx -> dir ); */ /* temp_dir.c */
    }
    return rc;
}

static rc_t check_for_already_existing_output_file( const tool_ctx_t * tool_ctx )
{
    rc_t rc = 0;
    if ( !tool_ctx -> append && !tool_ctx -> force )
    {
        if ( file_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename ) ) /* helper.c */
        {
            rc = RC( rcExe, rcFile, rcPacking, rcName, rcExists );
            ErrMsg( "creating ouput-file '%s' -> %R", tool_ctx -> output_filename, rc );
        }
    }
    return rc;
}

/* -------------------------------------------------------------------------------------------- */

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( rc != 0 )
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    if ( rc == 0 )
    {
        uint32_t param_count;
        rc = ArgsParamCount( args, &param_count );
        if ( rc != 0 )
            ErrMsg( "ArgsParamCount() -> %R", rc );
        else
        {
            if ( param_count == 0 || param_count > 1 )
                rc = Usage ( args );
            else
            {
                tool_ctx_t tool_ctx;
                rc = populate_tool_ctx( &tool_ctx, args ); /* above */
                if ( rc == 0 )
                {
                    rc = check_for_already_existing_output_file( &tool_ctx );
                    if ( rc == 0 )
                        rc = perform_tool( &tool_ctx );     /* above */

                    KDirectoryRelease( tool_ctx . dir );
                    destroy_temp_dir( tool_ctx . temp_dir ); /* temp_dir.c */
                }
            }
        }
    }
    return rc;
}
