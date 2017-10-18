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
#include "concatenator.h"
#include "cleanup_task.h"

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/out.h>
#include <klib/printf.h>
#include <kfs/directory.h>
#include <kproc/procmgr.h>

#include <stdio.h>
#include <os-native.h>
#include <sysalloc.h>

static const char * format_usage[] = { "format (special, fastq, lookup, default=special)", NULL };
#define OPTION_FORMAT   "format"
#define ALIAS_FORMAT    "F"

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

static const char * detail_usage[] = { "print details", NULL };
#define OPTION_DETAILS  "details"
#define ALIAS_DETAILS    "x"

static const char * split_usage[] = { "split fastq-output", NULL };
#define OPTION_SPLIT     "split"
#define ALIAS_SPLIT      "s"

static const char * stdout_usage[] = { "print output to stdout", NULL };
#define OPTION_STDOUT    "stdout"
#define ALIAS_STDOUT     "Z"

static const char * gzip_usage[] = { "compress output using gzip", NULL };
#define OPTION_GZIP      "gzip"
#define ALIAS_GZIP       "g"

static const char * bzip2_usage[] = { "compress output using bzip2", NULL };
#define OPTION_BZIP2     "bzip2"
#define ALIAS_BZIP2      "z"

static const char * force_usage[] = { "force to overwrite existing file(s)", NULL };
#define OPTION_FORCE     "force"
#define ALIAS_FORCE      "f"

static const char * maxfd_usage[] = { "maximal number of file-descriptors", NULL };
#define OPTION_MAXFD     "maxfd"
#define ALIAS_MAXFD      "a"

OptDef ToolOptions[] =
{
    { OPTION_FORMAT,    ALIAS_FORMAT,    NULL, format_usage,     1, true,   false },
    { OPTION_OUTPUT,    ALIAS_OUTPUT,    NULL, output_usage,     1, true,   false },
    { OPTION_BUFSIZE,   ALIAS_BUFSIZE,   NULL, bufsize_usage,    1, true,   false },
    { OPTION_CURCACHE,  ALIAS_CURCACHE,  NULL, curcache_usage,   1, true,   false },
    { OPTION_MEM,       ALIAS_MEM,       NULL, mem_usage,        1, true,   false },
    { OPTION_TEMP,      ALIAS_TEMP,      NULL, temp_usage,       1, true,   false },
    { OPTION_THREADS,   ALIAS_THREADS,   NULL, threads_usage,    1, true,   false },
    { OPTION_PROGRESS,  ALIAS_PROGRESS,  NULL, progress_usage,   1, false,  false },
    { OPTION_DETAILS,   ALIAS_DETAILS,   NULL, detail_usage,     1, false,  false },
    { OPTION_SPLIT,     ALIAS_SPLIT,     NULL, split_usage,      1, false,  false },
    { OPTION_STDOUT,    ALIAS_STDOUT,    NULL, stdout_usage,     1, false,  false }, 
    { OPTION_GZIP,      ALIAS_GZIP,      NULL, gzip_usage,       1, false,  false },
    { OPTION_BZIP2,     ALIAS_BZIP2,     NULL, bzip2_usage,      1, false,  false },
    { OPTION_FORCE,     ALIAS_FORCE,     NULL, force_usage,      1, false,  false },
    { OPTION_MAXFD,     ALIAS_MAXFD,     NULL, maxfd_usage,      1, true,   false }    
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

#define HOSTNAMELEN 64
#define DFLT_PATH_LEN 4096

#define DFLT_MEM_LIMIT ( 1024L * 1024 * 50 )
#define MIN_MEM_LIMIT ( 1024L * 1024 * 5 )

#define DFLT_CUR_CACHE ( 5 * 1024 * 1024 )
#define DFLT_BUF_SIZE ( 1024 * 1024 )

typedef struct tool_ctx
{
    cmn_params cmn; /* cmn_iter.h */

    const char * lookup_filename;
    const char * output_filename;
    const char * index_filename;

    tmp_id tmp_id;

    char hostname[ HOSTNAMELEN ];
    char dflt_lookup[ DFLT_PATH_LEN ];
    char dflt_index[ DFLT_PATH_LEN ];
    char dflt_output[ DFLT_PATH_LEN ];
    
    struct KFastDumpCleanupTask * cleanup_task;
    
    size_t buf_size, mem_limit;

    uint32_t num_threads, max_fds;
    uint64_t total_ram;
    
    format_t fmt;

    compress_t compress;    

    bool remove_temp_path, print_to_stdout, force;
} tool_ctx;


static rc_t get_process_pid( uint32_t * pid )
{
    struct KProcMgr * proc_mgr;
    rc_t rc = KProcMgrMakeSingleton ( &proc_mgr );
    if ( rc != 0 )
        ErrMsg( "cannot access process-manager" );
    else
    {
        rc = KProcMgrGetPID ( proc_mgr, pid );
        if ( rc != 0 )
            ErrMsg( "cannot get process-id" );
        KProcMgrRelease ( proc_mgr );
    }
    return rc;
}

static rc_t get_hostname( tool_ctx * tool_ctx )
{
    struct KProcMgr * proc_mgr;
    rc_t rc = KProcMgrMakeSingleton ( &proc_mgr );
    if ( rc != 0 )
        ErrMsg( "cannot access process-manager" );
    else
    {
        rc = KProcMgrGetHostName ( proc_mgr, tool_ctx -> hostname, sizeof tool_ctx -> hostname );
        if ( rc != 0 )
        {
            size_t num_writ;
            rc = string_printf( tool_ctx -> hostname, sizeof tool_ctx -> hostname, &num_writ, "host" );
        }
        KProcMgrRelease ( proc_mgr );
    }
    
    if ( rc == 0 )
        tool_ctx -> tmp_id . hostname = ( const char * )&( tool_ctx -> hostname );
    return rc;
}


static rc_t show_details( tool_ctx * tool_ctx )
{
    rc_t rc = KOutMsg( "cursor-cache : %,ld\n", tool_ctx -> cmn . cursor_cache );
    if ( rc == 0 )
        rc = KOutMsg( "buf-size     : %,ld\n", tool_ctx -> buf_size );
    if ( rc == 0 )
        rc = KOutMsg( "mem-limit    : %,ld\n", tool_ctx -> mem_limit );
    if ( rc == 0 )
        rc = KOutMsg( "threads      : %d\n", tool_ctx -> num_threads );
    if ( rc == 0 )
        rc = KOutMsg( "max. fds     : %d\n", tool_ctx -> max_fds );     
    if ( rc == 0 )
        rc = KOutMsg( "scratch-path : '%s'\n", tool_ctx -> tmp_id . temp_path );
    if ( rc == 0 )
        rc = KOutMsg( "pid          : %u\n", tool_ctx -> tmp_id . pid );
    if ( rc == 0 )
        rc = KOutMsg( "hostname     : %s\n", tool_ctx -> tmp_id . hostname );
    if ( rc == 0 )
        rc = KOutMsg( "total RAM    : %,lu bytes\n", tool_ctx -> total_ram );
    if ( rc == 0 )
        rc = KOutMsg( "output-format: " );
    if ( rc == 0 )
    {
        switch ( tool_ctx -> fmt )
        {
            case ft_special     : rc = KOutMsg( "SPECIAL\n" ); break;
            case ft_fastq       : rc = KOutMsg( "FASTQ\n" ); break;
            case ft_fastq_split : rc = KOutMsg( "FASTQ split\n" ); break;
        }
    }
    if ( rc == 0 )
        rc = KOutMsg( "output       : '%s'\n", tool_ctx -> output_filename );
    return rc;
}

/* taken form libs/kapp/main-priv.h */
rc_t KAppGetTotalRam ( uint64_t * totalRam );

static const char * dflt_temp_path = "./fast.tmp";
#define DFLT_MAX_FD 32
#define DFLT_NUM_THREADS 1

static rc_t populate_tool_ctx( tool_ctx * tool_ctx, Args * args )
{
    rc_t rc = ArgsParamValue( args, 0, ( const void ** )&( tool_ctx -> cmn . acc ) );
    if ( rc != 0 )
        ErrMsg( "ArgsParamValue() -> %R", rc );
    else
    {
        tool_ctx -> compress = get_compress_t( get_bool_option( args, OPTION_GZIP ),
                                                get_bool_option( args, OPTION_BZIP2 ) ); /* helper.c */
        
        tool_ctx -> cmn . cursor_cache = get_size_t_option( args, OPTION_CURCACHE, DFLT_CUR_CACHE );            
        tool_ctx -> cmn . show_progress = get_bool_option( args, OPTION_PROGRESS );
        tool_ctx -> cmn . show_details = get_bool_option( args, OPTION_DETAILS );
        tool_ctx -> cmn . row_count = 0;

        tool_ctx -> tmp_id . temp_path = get_str_option( args, OPTION_TEMP, NULL );
        tool_ctx -> print_to_stdout = get_bool_option( args, OPTION_STDOUT );
        tool_ctx -> force = get_bool_option( args, OPTION_FORCE );        
        tool_ctx -> remove_temp_path = false;
        tool_ctx -> output_filename = get_str_option( args, OPTION_OUTPUT, NULL );
        tool_ctx -> lookup_filename = NULL;
        tool_ctx -> index_filename = NULL;
        tool_ctx -> buf_size = get_size_t_option( args, OPTION_BUFSIZE, DFLT_BUF_SIZE );
        tool_ctx -> mem_limit = get_size_t_option( args, OPTION_MEM, DFLT_MEM_LIMIT );
        tool_ctx -> num_threads = get_uint64_t_option( args, OPTION_THREADS, DFLT_NUM_THREADS );
        tool_ctx -> max_fds = get_uint64_t_option( args, OPTION_MAXFD, DFLT_MAX_FD );
        tool_ctx -> fmt = get_format_t( get_str_option( args, OPTION_FORMAT, NULL ) ); /* helper.c */

        if ( tool_ctx -> mem_limit < MIN_MEM_LIMIT )
            tool_ctx -> mem_limit = MIN_MEM_LIMIT;
            
        if ( get_bool_option( args, OPTION_SPLIT ) && tool_ctx -> fmt == ft_fastq )
            tool_ctx -> fmt = ft_fastq_split;

        if ( tool_ctx -> print_to_stdout && tool_ctx -> cmn . show_progress )
            tool_ctx -> cmn . show_progress = false;

        if ( tool_ctx -> print_to_stdout && tool_ctx -> cmn . show_details )
            tool_ctx -> cmn . show_details = false;
    }

    if ( rc == 0 )
    {
        rc = KAppGetTotalRam ( &( tool_ctx -> total_ram ) );
        if ( rc != 0 )
            ErrMsg( "KAppGetTotalRam() -> %R", rc );
    }
    
    if ( rc == 0 )
    {
        rc = KDirectoryNativeDir( &( tool_ctx -> cmn . dir ) );
        if ( rc != 0 )
            ErrMsg( "KDirectoryNativeDir() -> %R", rc );
    }
    
    if ( rc == 0 )
        rc = get_process_pid( &( tool_ctx -> tmp_id . pid ) );

    if ( rc == 0 )
        rc = get_hostname( tool_ctx );

    /* handle the important temp-path ( user gave a specific one - or not )*/
    if ( rc == 0 )
    {
        uint32_t pt;
        if ( tool_ctx -> tmp_id . temp_path == NULL )
        {
            /* if the user did not give us a temp-path: use the dflt-path and clean up after use */
            tool_ctx -> tmp_id . temp_path = dflt_temp_path;
            pt = KDirectoryPathType( tool_ctx -> cmn . dir, "%s", tool_ctx -> tmp_id . temp_path );
            if ( pt != kptDir )
            {
                rc = KDirectoryCreateDir ( tool_ctx -> cmn . dir, 0775, kcmInit, "%s", tool_ctx -> tmp_id . temp_path );
                if ( rc != 0 )
                    ErrMsg( "scratch-path '%s' cannot be created!", tool_ctx -> tmp_id . temp_path );
                else
                    tool_ctx -> remove_temp_path = true;
            }
        }
        else
        {
            /* if the user did give us a temp-path: try to create it if not force-options is given */
            KCreateMode create_mode = tool_ctx -> force ? kcmInit : kcmCreate;
            rc = KDirectoryCreateDir ( tool_ctx -> cmn . dir, 0775, create_mode, "%s", tool_ctx -> tmp_id . temp_path );
            if ( rc != 0 )
                ErrMsg( "scratch-path '%s' cannot be created!", tool_ctx -> tmp_id . temp_path );
        }
    }
    
    if ( rc == 0 )
    {
        uint32_t temp_path_len = string_measure( tool_ctx -> tmp_id . temp_path, NULL );
        tool_ctx -> tmp_id . temp_path_ends_in_slash = ( tool_ctx -> tmp_id . temp_path[ temp_path_len - 1 ] == '/' );
    }

    tool_ctx -> dflt_lookup[ 0 ] = 0;
    tool_ctx -> dflt_index[ 0 ] = 0;
    tool_ctx -> dflt_output[ 0 ] = 0;

    if ( rc == 0 )
    {
        /* generate the full path of the lookup-table */
        rc = make_pre_and_post_fixed( tool_ctx -> dflt_lookup, sizeof tool_ctx -> dflt_lookup,
                      tool_ctx -> cmn . acc,
                      &( tool_ctx -> tmp_id ),
                      "lookup" ); /* helper.c */
        if ( rc == 0 )
            tool_ctx -> lookup_filename = tool_ctx -> dflt_lookup;
    }
    
    
    if ( rc == 0 )
    {
        /* generate the full path of the lookup-index-table */                
        rc = make_pre_and_post_fixed( tool_ctx -> dflt_index, sizeof tool_ctx -> dflt_index,
                  tool_ctx -> cmn . acc,
                  &( tool_ctx -> tmp_id ),                              
                  "lookup.idx" ); /* helper.c */
        if ( rc == 0 )
            tool_ctx -> index_filename = tool_ctx -> dflt_index;
    }
    
    if ( rc == 0 && tool_ctx -> output_filename == NULL )
    {
        /* generate the full path of the output-file, if not given */
        rc = make_postfixed( tool_ctx -> dflt_output, sizeof tool_ctx -> dflt_output,
                             tool_ctx -> cmn . acc,
                             ".fastq" ); /* helper.c */
        if ( rc == 0 )
            tool_ctx -> output_filename = tool_ctx -> dflt_output;
    }
    
    
    if ( rc == 0 )
        rc = Make_FastDump_Cleanup_Task ( &( tool_ctx -> cleanup_task ) );
    return rc;
}


/* --------------------------------------------------------------------------------------------
    produce the lookup-table by iterating over the PRIMARY_ALIGNMENT - table:
   -------------------------------------------------------------------------------------------- 
    reading SEQ_SPOT_ID, SEQ_READ_ID and RAW_READ
    SEQ_SPOT_ID and SEQ_READ_ID is merged into a 64-bit-key
    RAW_READ is read as 4na-unpacked ( Schema does not provide 4na-packed for this column )
    these key-pairs are temporarely stored in a KVector until a limit is reached
    after that limit is reached they are writen sorted into the file-system as sub-files
    These sub-files are than merge-sorted into the final output-file.
    This output-file is a binary data-file:
    content: [KEY][RAW_READ]
    KEY... 64-bit value as SEQ_SPOT_ID shifted left by 1 bit, zero-bit contains SEQ_READ_ID
    RAW_READ... 16-bit binary-chunk-lenght, followed by n bytes of packed 4na
-------------------------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------------------------
    produce special-output ( SPOT_ID,READ,SPOT_GROUP ) by iterating over the SEQUENCE - table:
    produce fastq-output by iterating over the SEQUENCE - table:
   -------------------------------------------------------------------------------------------- 
   each thread iterates over a slice of the SEQUENCE-table
   for each SPOT it may look up an entry in the lookup-table to get the READ if it is not stored in the SEQ-tbl
   
-------------------------------------------------------------------------------------------- */

static rc_t perform_join( tool_ctx * tool_ctx, locked_file_list * files )
{
    join_params jp;
    
    jp . dir              = tool_ctx -> cmn . dir;
    jp . accession        = tool_ctx -> cmn . acc;
    jp . lookup_filename  = tool_ctx -> lookup_filename;
    jp . index_filename   = tool_ctx -> index_filename;
    jp . output_filename  = tool_ctx -> output_filename;
    jp . tmp_id           = &( tool_ctx -> tmp_id );
    jp . join_progress    = NULL;
    jp . buf_size         = tool_ctx -> buf_size;
    jp . show_progress    = tool_ctx -> cmn . show_progress;
    jp . num_threads      = tool_ctx -> num_threads;
    jp . first_row        = 0;
    jp . row_count        = 0;
    jp . fmt              = tool_ctx -> fmt;
    jp . joined_files     = files -> files;
    jp . cleanup_task     = tool_ctx -> cleanup_task;
    
    return execute_join( &jp ); /* join.c */
}

static rc_t perform_concat( const tool_ctx * tool_ctx, locked_file_list * files )
{
    concat_params cp;
    
    cp . dir              = tool_ctx -> cmn . dir;
    cp . output_filename  = tool_ctx -> output_filename;
    cp . buf_size         = tool_ctx -> buf_size;
    cp . show_progress    = tool_ctx -> cmn.show_progress;
    cp . print_to_stdout  = tool_ctx -> print_to_stdout;
    cp . compress         = tool_ctx -> compress;
    cp . force            = tool_ctx -> force;
    cp . joined_files     = files -> files;
    cp . delete_files     = true;

    return execute_concat( &cp ); /* join.c */
}


static const uint32_t queue_timeout = 200;  /* ms */

static rc_t produce_lookup_files( tool_ctx * tool_ctx )
{
    struct background_file_merger * bg_file_merger;
    
    /* the background-file-merger catches the files produced by
       the background-vector-merger */
    rc_t rc = make_background_file_merger( &bg_file_merger,
                                tool_ctx -> cmn . dir,
                                & tool_ctx -> tmp_id,
                                tool_ctx -> cleanup_task,
                                tool_ctx -> lookup_filename,
                                tool_ctx -> index_filename,
                                tool_ctx -> num_threads,
                                queue_timeout,
                                tool_ctx -> buf_size );
    if ( rc == 0 )
    {
        struct background_vector_merger * bg_vec_merger;
    
        /* the background-vector-merger catches the KVectors produced by
           the lookup-produceer */
        rc = make_background_vector_merger( &bg_vec_merger,
                 tool_ctx -> cmn . dir,
                 & tool_ctx -> tmp_id,
                 tool_ctx -> cleanup_task,
                 bg_file_merger,
                 tool_ctx -> num_threads,
                 queue_timeout,
                 tool_ctx -> buf_size );
        
        /* the lookup-producer is the source of the chain */
        if ( rc == 0 )
        {

/* --------------------------------------------------------------------------------------------
    produce the lookup-table by iterating over the PRIMARY_ALIGNMENT - table:
   -------------------------------------------------------------------------------------------- 
    reading SEQ_SPOT_ID, SEQ_READ_ID and RAW_READ
    SEQ_SPOT_ID and SEQ_READ_ID is merged into a 64-bit-key
    RAW_READ is read as 4na-unpacked ( Schema does not provide 4na-packed for this column )
    these key-pairs are temporarely stored in a KVector until a limit is reached
    after that limit is reached they are writen sorted into the file-system as sub-files
    These sub-files are than merge-sorted into the final output-file.
    This output-file is a binary data-file:
    content: [KEY][RAW_READ]
    KEY... 64-bit value as SEQ_SPOT_ID shifted left by 1 bit, zero-bit contains SEQ_READ_ID
    RAW_READ... 16-bit binary-chunk-lenght, followed by n bytes of packed 4na
-------------------------------------------------------------------------------------------- */
            rc = execute_lookup_production( &( tool_ctx -> cmn ),
                                            bg_vec_merger,
                                            tool_ctx -> buf_size,
                                            tool_ctx -> mem_limit,
                                            tool_ctx -> num_threads );
    
            if ( rc == 0 )
                rc = seal_background_vector_merger( bg_vec_merger ); /* merge_sorter.h */
        }
        
        if ( rc == 0 )
        {
            rc = wait_for_background_vector_merger( bg_vec_merger );
            release_background_vector_merger( bg_vec_merger );
        }
        
        
        if ( rc == 0 )
            rc = seal_background_file_merger( bg_file_merger );

        if ( rc == 0 )
        {
            rc = wait_for_background_file_merger( bg_file_merger );
            release_background_file_merger( bg_file_merger );
        }
    }
    return rc;
}


/* -------------------------------------------------------------------------------------------- */


static rc_t produce_final_output( tool_ctx * tool_ctx )
{
    locked_file_list partial_results;
    
    rc_t rc = locked_file_list_init( &partial_results, 25 );
    
    /* STEP 3 : join SEQUENCE-table with lookup-table */
    if ( rc == 0 )
        rc = perform_join( tool_ctx, &partial_results ); /* <==== above! */

    /* from now on we do not need the lookup-file and it's index any more... */
    if ( tool_ctx -> dflt_lookup[ 0 ] != 0 )
        KDirectoryRemove( tool_ctx -> cmn . dir, true, "%s", tool_ctx -> dflt_lookup );

    if ( tool_ctx -> dflt_index[ 0 ] != 0 )
        KDirectoryRemove( tool_ctx -> cmn . dir, true, "%s", tool_ctx -> dflt_index );

    /* STEP 4 : concatenate output-chunks */
    if ( rc == 0 )
        rc = perform_concat( tool_ctx, &partial_results ); /* <==== above! */

    locked_file_list_release( &partial_results, tool_ctx -> cmn . dir );

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
        tool_ctx tool_ctx;
        rc = populate_tool_ctx( &tool_ctx, args );
        if ( rc == 0 )
        {
            if ( tool_ctx . cmn . show_details )
                rc = show_details( &tool_ctx );
            
            /* =================================================== */

            if ( rc == 0 )
                rc = produce_lookup_files( &tool_ctx );

            if ( rc == 0 )
                rc = produce_final_output( &tool_ctx );

            /* =================================================== */
            
            if ( tool_ctx . remove_temp_path )
            {
                rc_t rc1 = KDirectoryClearDir ( tool_ctx . cmn . dir, true,
                            "%s", tool_ctx . tmp_id . temp_path );
                if ( rc1 == 0 )
                    rc1 = KDirectoryRemove ( tool_ctx . cmn . dir, true,
                            "%s", tool_ctx . tmp_id . temp_path );
            }
            
            KDirectoryRelease( tool_ctx . cmn . dir );
        }
    }
    return rc;
}
