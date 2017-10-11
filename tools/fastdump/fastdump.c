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
    { OPTION_FORCE,     ALIAS_FORCE,     NULL, force_usage,      1, false,  false }
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
    bool remove_temp_path, print_to_stdout, force;
    compress_t compress;
    size_t buf_size, mem_limit;
    uint64_t num_threads;
    format_t fmt;
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
        tool_ctx -> num_threads = get_uint64_t_option( args, OPTION_THREADS, 1 );
        tool_ctx -> fmt = get_format_t( get_str_option( args, OPTION_FORMAT, NULL ) ); /* helper.c */

        if ( tool_ctx -> mem_limit < MIN_MEM_LIMIT )
            tool_ctx -> mem_limit = MIN_MEM_LIMIT;
            
        if ( get_bool_option( args, OPTION_SPLIT ) && tool_ctx -> fmt == ft_fastq )
            tool_ctx -> fmt = ft_fastq_split;

        if ( tool_ctx -> print_to_stdout && tool_ctx -> cmn . show_progress )
            tool_ctx -> cmn . show_progress = false;

        if ( tool_ctx -> print_to_stdout && tool_ctx -> cmn . show_details )
            tool_ctx -> cmn . show_details = false;
            
        rc = KDirectoryNativeDir( &( tool_ctx -> cmn . dir ) );
        if ( rc != 0 )
            ErrMsg( "KDirectoryNativeDir() -> %R", rc );
    }
    if ( rc == 0 )
        rc = get_process_pid( &( tool_ctx -> tmp_id . pid ) );
    if ( rc == 0 )
    {
        size_t num_writ;
        rc = string_printf( tool_ctx -> hostname, sizeof tool_ctx -> hostname, &num_writ, "host" );
        if ( rc == 0 )
        {
            tool_ctx -> tmp_id . hostname = ( const char * )&( tool_ctx -> hostname );
        }
    }
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
static rc_t perform_lookup_production( tool_ctx * tool_ctx, locked_file_list * files, struct background_merger * merger )
{
    lookup_production_params lp;

    lp . dir                = tool_ctx -> cmn . dir;
    lp . accession          = tool_ctx -> cmn . acc;
    lp . tmp_id             = &( tool_ctx -> tmp_id );
    lp . mem_limit          = tool_ctx -> mem_limit;
    lp . buf_size           = tool_ctx -> buf_size;
    lp . num_threads        = tool_ctx -> num_threads;
    lp . cmn                = &( tool_ctx -> cmn );
    lp . files              = files;
    lp . merger             = merger;
    
    return execute_lookup_production( &lp );
}


static rc_t perform_lookup_merge( tool_ctx * tool_ctx, locked_file_list * files )
{
    merge_sort_params mp;
    
    mp . dir                = tool_ctx -> cmn . dir;
    mp . lookup_filename    = tool_ctx -> lookup_filename;
    mp . index_filename     = tool_ctx -> index_filename;
    mp . tmp_id             = &( tool_ctx -> tmp_id );
    mp . num_threads        = tool_ctx -> num_threads;
    mp . show_progress      = tool_ctx -> cmn . show_progress;
    
    return execute_merge_sort( &mp, files );
}


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
        rc = KOutMsg( "scratch-path : '%s'\n", tool_ctx -> tmp_id . temp_path );
    if ( rc == 0 )
        rc = KOutMsg( "pid          : %u\n", tool_ctx -> tmp_id . pid );
    if ( rc == 0 )
        rc = KOutMsg( "hostname     : %s\n", tool_ctx -> tmp_id . hostname );
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

static const char * dflt_temp_path = "./fast.tmp";

static rc_t check_temp_path( tool_ctx * tool_ctx )
{
    rc_t rc = 0;
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
    if ( rc == 0 )
    {
        uint32_t temp_path_len = string_measure( tool_ctx -> tmp_id . temp_path, NULL );
        tool_ctx -> tmp_id . temp_path_ends_in_slash = ( tool_ctx -> tmp_id . temp_path[ temp_path_len - 1 ] == '/' );
    }
    return rc;
}

/* -------------------------------------------------------------------------------------------- */
/*
static rc_t CC write_to_FILE ( void *f, const char *buffer, size_t bytes, size_t *num_writ )
{
    * num_writ = fwrite ( buffer, 1, bytes, f );
    if ( * num_writ != bytes )
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    return 0;
}
*/
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
            rc = check_temp_path( &tool_ctx );
            if ( rc == 0  )
            {
                locked_file_list files;
                struct background_merger * merger;
                
                char dflt_lookup[ 4096 ];
                char dflt_index[ 4096 ];
                char dflt_output[ 4096 ];
            
                dflt_lookup[ 0 ] = 0;
                dflt_index[ 0 ] = 0;
                dflt_output[ 0 ] = 0;
            
                /* generate the full path of the lookup-table */
                rc = make_pre_and_post_fixed( dflt_lookup, sizeof dflt_lookup,
                              tool_ctx . cmn . acc,
                              &( tool_ctx . tmp_id ),
                              "lookup" ); /* helper.c */
                if ( rc == 0 )
                    tool_ctx . lookup_filename = dflt_lookup;

                if ( rc == 0 )
                    /* generate the full path of the lookup-index-table */                
                    rc = make_pre_and_post_fixed( dflt_index, sizeof dflt_index,
                              tool_ctx . cmn . acc,
                              &( tool_ctx . tmp_id ),                              
                              "lookup.idx" ); /* helper.c */
                if ( rc == 0 )
                    tool_ctx . index_filename = dflt_index;
                
                if ( rc == 0 && tool_ctx . output_filename == NULL )
                {
                    /* generate the full path of the output-file, if not given */
                    rc = make_postfixed( dflt_output, sizeof dflt_output,
                                         tool_ctx . cmn . acc,
                                         ".fastq" ); /* helper.c */
                    if ( rc == 0 )
                        tool_ctx . output_filename = dflt_output;
                }

                if ( rc == 0 && tool_ctx . cmn . show_details )
                    rc = show_details( &tool_ctx );

                if ( rc == 0 )
                    rc = init_locked_file_list( &files, 25 );

                /* ============================================================== */
                
                if ( rc == 0 )
                    rc = make_background_merger( &merger,
                             tool_ctx . cmn . dir,
                             &files,
                             & tool_ctx . tmp_id,
                             tool_ctx . num_threads,
                             200,
                             tool_ctx . buf_size );
                
                /* STEP 1 : produce lookup-table */
                if ( rc == 0 )
                    rc = perform_lookup_production( &tool_ctx, &files, merger ); /* <==== above */
                
                if ( rc == 0 )
                {
                    rc = wait_for_background_merger( merger );
                    release_background_merger( merger );
                }
                
                /* STEP 2 : merge the lookup-tables */
                if ( rc == 0 )
                    rc = perform_lookup_merge( &tool_ctx, &files ); /* <==== above */
                
                if ( rc == 0 )
                {
                    rc = delete_files( tool_ctx . cmn . dir, files . files );
                    release_locked_file_list( &files );
                }
                if ( rc == 0 )
                    rc = init_locked_file_list( &files, 25 );
                
                /* STEP 3 : join SEQUENCE-table with lookup-table */
                if ( rc == 0 )
                    rc = perform_join( &tool_ctx, &files ); /* <==== above! */

                /* from now on we do not need the lookup-file and it's index any more... */
                if ( dflt_lookup[ 0 ] != 0 )
                    KDirectoryRemove( tool_ctx . cmn . dir, true, "%s", dflt_lookup );

                if ( dflt_index[ 0 ] != 0 )
                    KDirectoryRemove( tool_ctx . cmn . dir, true, "%s", dflt_index );

                /* STEP 4 : concatenate output-chunks */
                if ( rc == 0 )
                    rc = perform_concat( &tool_ctx, &files ); /* <==== above! */

                /* ============================================================== */
                
                if ( tool_ctx . remove_temp_path )
                {
                    rc_t rc1 = KDirectoryClearDir ( tool_ctx . cmn . dir, true,
                                "%s", tool_ctx . tmp_id . temp_path );
                    if ( rc1 == 0 )
                        rc1 = KDirectoryRemove ( tool_ctx . cmn . dir, true,
                                "%s", tool_ctx . tmp_id . temp_path );
                }
                
                release_locked_file_list( &files );
            }
            KDirectoryRelease( tool_ctx . cmn . dir );
        }
    }
    return rc;
}
