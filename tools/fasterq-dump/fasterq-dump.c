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

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_arg_helper_
#include "arg_helper.h"
#endif

#ifndef _h_inspector_
#include "inspector.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_sorter_
#include "sorter.h"
#endif

#ifndef _h_join_
#include "join.h"
#endif

#ifndef _h_tbl_join_
#include "tbl_join.h"
#endif

#ifndef _h_kapp_main_
#include <kapp/main.h>
#endif

#ifndef _h_kapp_args_
#include <kapp/args.h>
#endif

#ifndef _h_kfg_config_
#include <kfg/config.h> /* KConfigSetNgcFile */
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

/* ---------------------------------------------------------------------------------- */

static const char * format_usage[] = { "format (special, fastq, lookup, default=fastq)", NULL };
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

static const char * stdout_usage[] = { "print output to stdout", NULL };
#define OPTION_STDOUT    "stdout"
#define ALIAS_STDOUT     "Z"

static const char * force_usage[] = { "force to overwrite existing file(s)", NULL };
#define OPTION_FORCE     "force"
#define ALIAS_FORCE      "f"

static const char * skip_tech_usage[] = { "skip technical reads", NULL };
#define OPTION_SKIP_TECH      "skip-technical"

static const char * incl_tech_usage[] = { "include technical reads", NULL };
#define OPTION_INCL_TECH      "include-technical"

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
#define OPTION_APPEND           "append"
#define ALIAS_APPEND            "A"

static const char * fasta_usage[] = { "produce FASTA output", NULL };
#define OPTION_FASTA            "fasta"

static const char * fasta_us_usage[] = { "produce FASTA output, unsorted", NULL };
#define OPTION_FASTA_US         "fasta-unsorted"

static const char * seq_defline_usage[] = { "custom defline for sequence: ",
                                            "$ac=accession, $sn=spot-name, ",
                                            "$sg=spot-group, $si=spot-id, ",
                                            "$ri=read-id, $rl=read-length",                                            
                                            NULL };
#define OPTION_SEQ_DEFLINE      "seq-defline"

static const char * qual_defline_usage[] = { "custom defline for qualities: ",
                                             "same as seq-defline",
                                             NULL };
#define OPTION_QUAL_DEFLINE     "qual-defline"

static const char * only_un_usage[] = { "process only unaligned reads", NULL };
#define OPTION_ONLY_UN          "only-unaligned"
#define ALIAS_ONLY_UN           "U"

static const char * only_a_usage[] = { "process only aligned reads", NULL };
#define OPTION_ONLY_ALIG        "only-aligned"
#define ALIAS_ONLY_ALIG         "a"

static const char * limit_usage[] = { "limit rowcount per thread", NULL };
#define OPTION_LIMIT            "limit"
#define ALIAS_LIMIT             "l"

static const char * ngc_usage[] = { "PATH to ngc file", NULL };
#define OPTION_NGC              "ngc"

/* ---------------------------------------------------------------------------------- */

OptDef ToolOptions[] = {
    { OPTION_FORMAT,        ALIAS_FORMAT,       NULL, format_usage,         1, true,   false },
    { OPTION_OUTPUT_F,      ALIAS_OUTPUT_F,     NULL, outputf_usage,        1, true,   false },
    { OPTION_OUTPUT_D,      ALIAS_OUTPUT_D,     NULL, outputd_usage,        1, true,   false },
    { OPTION_BUFSIZE,       ALIAS_BUFSIZE,      NULL, bufsize_usage,        1, true,   false },
    { OPTION_CURCACHE,      ALIAS_CURCACHE,     NULL, curcache_usage,       1, true,   false },
    { OPTION_MEM,           ALIAS_MEM,          NULL, mem_usage,            1, true,   false },
    { OPTION_TEMP,          ALIAS_TEMP,         NULL, temp_usage,           1, true,   false },
    { OPTION_THREADS,       ALIAS_THREADS,      NULL, threads_usage,        1, true,   false },
    { OPTION_PROGRESS,      ALIAS_PROGRESS,     NULL, progress_usage,       1, false,  false },
    { OPTION_DETAILS,       ALIAS_DETAILS,      NULL, detail_usage,         1, false,  false },
    { OPTION_SPLIT_SPOT,    ALIAS_SPLIT_SPOT,   NULL, split_spot_usage,     1, false,  false },
    { OPTION_SPLIT_FILE,    ALIAS_SPLIT_FILE,   NULL, split_file_usage,     1, false,  false },
    { OPTION_SPLIT_3,       ALIAS_SPLIT_3,      NULL, split_3_usage,        1, false,  false },
    { OPTION_WHOLE_SPOT,    NULL,               NULL, whole_spot_usage,     1, false,  false },    
    { OPTION_STDOUT,        ALIAS_STDOUT,       NULL, stdout_usage,         1, false,  false },
    { OPTION_FORCE,         ALIAS_FORCE,        NULL, force_usage,          1, false,  false },
    { OPTION_SKIP_TECH,     NULL,               NULL, skip_tech_usage,      1, false,  false },
    { OPTION_INCL_TECH,     NULL,               NULL, incl_tech_usage,      1, false,  false },
    { OPTION_MINRDLEN,      ALIAS_MINRDLEN,     NULL, min_rl_usage,         1, true,   false },
    { OPTION_TABLE,         NULL,               NULL, table_usage,          1, true,   false },
    { OPTION_STRICT,        NULL,               NULL, strict_usage,         1, false,  false },
    { OPTION_BASE_FLT,      ALIAS_BASE_FLT,     NULL, base_flt_usage,       10, true,  false },
    { OPTION_APPEND,        ALIAS_APPEND,       NULL, append_usage,         1, false,  false },
    { OPTION_FASTA,         NULL,               NULL, fasta_usage,          1, false,  false },
    { OPTION_FASTA_US,      NULL,               NULL, fasta_us_usage,       1, false,  false },
    { OPTION_SEQ_DEFLINE,   NULL,               NULL, seq_defline_usage,    1, true,   false },
    { OPTION_QUAL_DEFLINE,  NULL,               NULL, qual_defline_usage,   1, true,   false },
    { OPTION_ONLY_UN,       ALIAS_ONLY_UN,      NULL, only_un_usage,        1, false,  false },
    { OPTION_ONLY_ALIG,     ALIAS_ONLY_ALIG,    NULL, only_a_usage,         1, false,  false },
    { OPTION_LIMIT,         ALIAS_LIMIT,        NULL, limit_usage,          1, true,   false },    
    { OPTION_NGC,           NULL,               NULL, ngc_usage,            1, true,   false },
};

/* ----------------------------------------------------------------------------------- */

const char UsageDefaultName[] = "fasterq-dump";

/* ----------------------------------------------------------------------------------- */

rc_t CC UsageSummary( const char * progname ) {
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [options]\n"
                     "\n", progname );
}

/* ----------------------------------------------------------------------------------- */

rc_t CC Usage ( const Args * args ) {
    rc_t rc;
    uint32_t idx, count = ( sizeof ToolOptions ) / ( sizeof ToolOptions[ 0 ] );
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;

    if ( NULL == args ) {
        rc = RC( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    } else {
        rc = ArgsProgram( args, &fullpath, &progname );
    }

    if ( 0 != rc ) {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary( progname );

    KOutMsg( "Options:\n" );
    for ( idx = 1; idx < count; ++idx ) { /* start with 1, do not advertize row-range-option*/
        const OptDef * opt = &ToolOptions[ idx ];
        const char * param = NULL;

        assert( opt );
        if ( 0 == strcmp( opt -> name, OPTION_NGC ) ) { param = "PATH"; }
        HelpOptionLine( opt -> aliases, opt -> name, param, opt -> help );
    }
    
    KOutMsg( "\n" );
    HelpOptionsStandard();

    KOutMsg( "for more information visit:\n" );
    KOutMsg( "   https://github.com/ncbi/sra-tools/wiki/HowTo:-fasterq-dump\n" );
    KOutMsg( "   https://github.com/ncbi/sra-tools/wiki/08.-prefetch-and-fasterq-dump\n" );

    HelpVersion( fullpath, KAppVersion() );

    return rc;
}

/* -------------------------------------------------------------------------------------------- */

#define DFLT_PATH_LEN 4096

typedef struct tool_ctx_t {
    KDirectory * dir;
    const VDBManager * vdb_mgr;     /* created, but unused to avoid race-condition in threads */

    const char * requested_temp_path;
    const char * accession_path;
    const char * accession_short;
    const char * output_filename;
    const char * output_dirname;
    const char * requested_seq_tbl_name;
    const char * seq_defline;
    const char * qual_defline;

    struct temp_dir_t * temp_dir; /* temp_dir.h */
    
    char lookup_filename[ DFLT_PATH_LEN ];
    char index_filename[ DFLT_PATH_LEN ];
    char dflt_output[ DFLT_PATH_LEN ];
    
    struct KFastDumpCleanupTask_t * cleanup_task; /* cleanup_task.h */
    
    size_t cursor_cache, buf_size, mem_limit;

    uint32_t num_threads /*, max_fds */;
    uint64_t total_ram;
    uint64_t row_limit;
    
    format_t fmt; /* helper.h */

    bool force, show_progress, show_details, append, use_stdout, only_unaligned, only_aligned;
    
    join_options_t join_options; /* helper.h */
} tool_ctx_t;

/* taken form libs/kapp/main-priv.h */
rc_t KAppGetTotalRam ( uint64_t * totalRam );

static rc_t get_environment( tool_ctx_t * tool_ctx ) {
    rc_t rc = KAppGetTotalRam ( &( tool_ctx -> total_ram ) );
    if ( 0 != rc ) {
        ErrMsg( "KAppGetTotalRam() -> %R", rc );
    } else {
        rc = KDirectoryNativeDir( &( tool_ctx -> dir ) );
        if ( 0 != rc ) {
            ErrMsg( "KDirectoryNativeDir() -> %R", rc );
        }
    }
    return rc;
}

const char * DFLT_SEQ_DEFLINE_FASTQ = "-";
const char * DFLT_SEQ_DEFLINE_FASTA = "-";
static const char * x_dflt_seq_defline( const tool_ctx_t * tool_ctx ) {
    return ( is_format_fasta( tool_ctx -> fmt ) ) /* helper.c */
        ? DFLT_SEQ_DEFLINE_FASTA : DFLT_SEQ_DEFLINE_FASTQ;
}

const char * DFLT_QUAL_DEFLINE_FASTQ = "-";
const char * DFLT_QUAL_DEFLINE_FASTA = "-";
static const char * x_dflt_qual_defline( const tool_ctx_t * tool_ctx ) {
    return ( is_format_fasta( tool_ctx -> fmt ) ) /* helper.c */
        ? DFLT_QUAL_DEFLINE_FASTA : DFLT_QUAL_DEFLINE_FASTQ;  
}

static rc_t show_details( const tool_ctx_t * tool_ctx,
                          const inspector_output_t * insp_output ) {
    rc_t rc = KOutHandlerSetStdErr();
    
    if ( 0 == rc ) {
        rc = KOutMsg( "cursor-cache : %,lu bytes\n", tool_ctx -> cursor_cache );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "buf-size     : %,lu bytes\n", tool_ctx -> buf_size );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "mem-limit    : %,lu bytes\n", tool_ctx -> mem_limit );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "threads      : %u\n", tool_ctx -> num_threads );
    }
    if ( 0 == rc && tool_ctx -> row_limit > 0 ) {
        rc = KOutMsg( "row-limit    : %lu\n", tool_ctx -> row_limit );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "scratch-path : '%s'\n", get_temp_dir( tool_ctx -> temp_dir ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "output-format: " );
    }
    if ( 0 == rc ) {
        switch ( tool_ctx -> fmt ) {
            case ft_unknown             : rc = KOutMsg( "unknown format\n" ); break;
            case ft_fastq_whole_spot    : rc = KOutMsg( "FASTQ whole spot\n" ); break;
            case ft_fastq_split_spot    : rc = KOutMsg( "FASTQ split spot\n" ); break;
            case ft_fastq_split_file    : rc = KOutMsg( "FASTQ split file\n" ); break;
            case ft_fastq_split_3       : rc = KOutMsg( "FASTQ split 3\n" ); break;
            case ft_fasta_whole_spot    : rc = KOutMsg( "FASTA whole spot\n" ); break;
            case ft_fasta_split_spot    : rc = KOutMsg( "FASTA split spot\n" ); break;
            case ft_fasta_us_split_spot : rc = KOutMsg( "FASTA-unsorted split spot\n" ); break;
            case ft_fasta_split_file    : rc = KOutMsg( "FASTA split file\n" ); break;
            case ft_fasta_split_3       : rc = KOutMsg( "FASTA split 3\n" ); break;
        }
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "output-file  : '%s'\n",
                    NULL != tool_ctx -> output_filename ? tool_ctx -> output_filename : "-" );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "output-dir   : '%s'\n", 
                    NULL != tool_ctx -> output_dirname ? tool_ctx -> output_dirname : "." );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "append-mode  : '%s'\n", tool_ctx -> append ? "YES" : "NO" );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "stdout-mode  : '%s'\n", tool_ctx -> append ? "YES" : "NO" );
    }
    if ( 0 == rc ) {
        const char * s = tool_ctx -> seq_defline;
        if ( NULL == s ) { s = x_dflt_seq_defline( tool_ctx ); }
        rc = KOutMsg( "seq-defline  : '%s'\n", s );
    }
    if ( 0 == rc ) {
        const char * s = tool_ctx -> qual_defline;
        if ( NULL == s ) { s = x_dflt_qual_defline( tool_ctx ); }
        rc = KOutMsg( "qual-defline  : '%s'\n", s );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "only-unaligned : '%s'\n", tool_ctx -> only_unaligned ? "YES" : "NO" );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "only-aligned  : '%s'\n", tool_ctx -> only_aligned ? "YES" : "NO" );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "accession     : '%s'\n", tool_ctx -> accession_short );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "accession-path: '%s'\n", tool_ctx -> accession_path );
    }

    if ( 0 == rc ) {
        rc = KOutMsg( "is-remote?    :'%s'\n", insp_output -> is_remote ? "YES" : "NO" );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "acc-size      : %,lu\n", insp_output -> acc_size );
    }

    if ( 0 == rc ) {
        rc = KOutMsg( "\n" );
    }
    KOutHandlerSetStdOut();
    return rc;
}

static const char * dflt_seq_tabl_name = "SEQUENCE";

#define DFLT_CUR_CACHE ( 5 * 1024 * 1024 )
#define DFLT_BUF_SIZE ( 1024 * 1024 )
#define DFLT_MEM_LIMIT ( 1024L * 1024 * 50 )
#define DFLT_NUM_THREADS 6
static rc_t get_user_input( tool_ctx_t * tool_ctx, const Args * args ) {
    rc_t rc = 0;
    bool split_spot, split_file, split_3, whole_spot, fasta, fasta_us;

    tool_ctx -> cursor_cache = get_size_t_option( args, OPTION_CURCACHE, DFLT_CUR_CACHE );
    tool_ctx -> show_progress = get_bool_option( args, OPTION_PROGRESS );
    tool_ctx -> show_details = get_bool_option( args, OPTION_DETAILS );
    tool_ctx -> requested_temp_path = get_str_option( args, OPTION_TEMP, NULL );
    tool_ctx -> force = get_bool_option( args, OPTION_FORCE );        
    tool_ctx -> output_filename = get_str_option( args, OPTION_OUTPUT_F, NULL );
    tool_ctx -> output_dirname = get_str_option( args, OPTION_OUTPUT_D, NULL );
    tool_ctx -> buf_size = get_size_t_option( args, OPTION_BUFSIZE, DFLT_BUF_SIZE );
    tool_ctx -> mem_limit = get_size_t_option( args, OPTION_MEM, DFLT_MEM_LIMIT );
    tool_ctx -> row_limit = get_uint64_t_option( args, OPTION_LIMIT, 0 );
    tool_ctx -> num_threads = get_uint32_t_option( args, OPTION_THREADS, DFLT_NUM_THREADS );
    tool_ctx -> join_options . rowid_as_name = false;
    tool_ctx -> join_options . skip_tech = !( get_bool_option( args, OPTION_INCL_TECH ) );
    tool_ctx -> join_options . min_read_len = get_uint32_t_option( args, OPTION_MINRDLEN, 0 );
    tool_ctx -> join_options . filter_bases = get_str_option( args, OPTION_BASE_FLT, NULL );

#if 0
    tool_ctx -> join_options . terminate_on_invalid = get_bool_option( args, OPTION_STRICT );
#else
    tool_ctx -> join_options . terminate_on_invalid = true;
#endif

    split_spot = get_bool_option( args, OPTION_SPLIT_SPOT );
    split_file = get_bool_option( args, OPTION_SPLIT_FILE );
    split_3    = get_bool_option( args, OPTION_SPLIT_3 );
    whole_spot = get_bool_option( args, OPTION_WHOLE_SPOT );
    fasta      = get_bool_option( args, OPTION_FASTA );
    fasta_us   = get_bool_option( args, OPTION_FASTA_US );

    if ( split_spot && split_file ) {
        ErrMsg( "split-spot and split-file exclude each other -> %R", rc );
    }

    tool_ctx -> fmt = get_format_t( get_str_option( args, OPTION_FORMAT, NULL ),
                    split_spot, split_file, split_3, whole_spot, fasta, fasta_us ); /* helper.c */
    if ( ft_fastq_split_3 == tool_ctx -> fmt ) {
        tool_ctx -> join_options . skip_tech = true;
    }

    tool_ctx -> requested_seq_tbl_name = get_str_option( args, OPTION_TABLE, dflt_seq_tabl_name );
    tool_ctx -> append = get_bool_option( args, OPTION_APPEND );
    tool_ctx -> use_stdout = get_bool_option( args, OPTION_STDOUT );

    tool_ctx -> seq_defline = get_str_option( args, OPTION_SEQ_DEFLINE, NULL );
    tool_ctx -> qual_defline = get_str_option( args, OPTION_QUAL_DEFLINE, NULL );    
    tool_ctx -> only_unaligned = get_bool_option( args, OPTION_ONLY_UN );
    tool_ctx -> only_aligned = get_bool_option( args, OPTION_ONLY_ALIG );
    
    {
        const char * ngc = get_str_option( args, OPTION_NGC, NULL );
        if ( NULL != ngc ) {
            KConfigSetNgcFile( ngc );
        }
    }
    return rc;
}

#define DFLT_MAX_FD 32
#define MIN_NUM_THREADS 2
#define MIN_MEM_LIMIT ( 1024L * 1024 * 5 )
#define MAX_BUF_SIZE ( 1024L * 1024 * 1024 )
static void encforce_constrains( tool_ctx_t * tool_ctx )
{
    uint32_t env_thread_count = get_env_u32( "DLFT_THREAD_COUNT", 0 );
    if ( env_thread_count > 0  ) {
        tool_ctx -> num_threads = env_thread_count;
    } else {
        if ( tool_ctx -> num_threads < MIN_NUM_THREADS ) {
            tool_ctx -> num_threads = MIN_NUM_THREADS;
        }
    }

    if ( tool_ctx -> mem_limit < MIN_MEM_LIMIT ) {
        tool_ctx -> mem_limit = MIN_MEM_LIMIT;
    }
    if ( tool_ctx -> buf_size > MAX_BUF_SIZE ) {
        tool_ctx -> buf_size = MAX_BUF_SIZE;
    }
    if ( tool_ctx -> use_stdout ) {
        switch( tool_ctx -> fmt ) {
            case ft_unknown             : break;

            case ft_fastq_whole_spot    : break;
            case ft_fastq_split_spot    : break;
            case ft_fastq_split_file    : tool_ctx -> use_stdout = false; break;
            case ft_fastq_split_3       : tool_ctx -> use_stdout = false; break;

            case ft_fasta_whole_spot    : break;
            case ft_fasta_split_spot    : break;
            case ft_fasta_us_split_spot : break;
            case ft_fasta_split_file    : tool_ctx -> use_stdout = false; break;
            case ft_fasta_split_3       : tool_ctx -> use_stdout = false; break;
        }
    }
    if ( tool_ctx -> use_stdout ) {
        tool_ctx -> force = false;
        tool_ctx -> append = false;
    }
    if ( tool_ctx -> only_aligned && tool_ctx -> only_unaligned ) {
        tool_ctx -> only_aligned = false;
        tool_ctx -> only_unaligned = false;
    }
}

static rc_t handle_accession( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    tool_ctx -> accession_short = inspector_extract_acc_from_path( tool_ctx -> accession_path ); /* inspector.c */

    if ( NULL == tool_ctx -> accession_short ) {
        rc = RC( rcApp, rcArgv, rcAccessing, rcParam, rcInvalid );
        ErrMsg( "accession '%s' invalid", tool_ctx -> accession_path );
    }
    return rc;
}

static rc_t handle_lookup_path( tool_ctx_t * tool_ctx ) {
    rc_t rc = generate_lookup_filename( tool_ctx -> temp_dir,
                                        &tool_ctx -> lookup_filename[ 0 ],
                                        sizeof tool_ctx -> lookup_filename );
    if ( 0 != rc ) {
        ErrMsg( "fasterq-dump.c handle_lookup_path( lookup_filename ) -> %R", rc );
    } else {
        size_t num_writ;
        /* generate the full path of the lookup-index-table */                
        rc = string_printf( &tool_ctx -> index_filename[ 0 ], sizeof tool_ctx -> index_filename,
                            &num_writ,
                            "%s.idx",
                            &tool_ctx -> lookup_filename[ 0 ] );
        if ( 0 != rc ) {
            ErrMsg( "fasterq-dump.c handle_lookup_path( index_filename ) -> %R", rc );
        }
    }
    return rc;
}

static bool fasta_requested( tool_ctx_t * tool_ctx ) {
    bool res = false;
    switch( tool_ctx -> fmt ) {
        case ft_unknown             : break;
        case ft_fastq_whole_spot    : break;
        case ft_fastq_split_spot    : break;
        case ft_fastq_split_file    : break;
        case ft_fastq_split_3       : break;

        case ft_fasta_whole_spot    : res = true; break;
        case ft_fasta_split_spot    : res = true; break;
        case ft_fasta_us_split_spot : res = true; break;
        case ft_fasta_split_file    : res = true; break;
        case ft_fasta_split_3       : res = true; break;
    }
    return res;
}

/* we have NO output-dir and NO output-file */
static rc_t make_output_filename_from_accession( tool_ctx_t * tool_ctx ) {
    rc_t rc;
    /* we DO NOT have a output-directory : build output-filename from the accession */
    /* generate the full path of the output-file, if not given */
    size_t num_writ;

    if ( fasta_requested( tool_ctx ) ) {
        rc = string_printf( &tool_ctx -> dflt_output[ 0 ], sizeof tool_ctx -> dflt_output,
                            &num_writ,
                            "%s.fasta",
                            tool_ctx -> accession_short );
    } else {
        rc = string_printf( &tool_ctx -> dflt_output[ 0 ], sizeof tool_ctx -> dflt_output,
                            &num_writ,
                            "%s.fastq",
                            tool_ctx -> accession_short );
    }

    if ( 0 != rc ) {
        ErrMsg( "string_printf( output-filename ) -> %R", rc );
    } else {
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
    }
    return rc;
}

/* we have an output-dir and NO output-file */
static rc_t make_output_filename_from_dir_and_accession( tool_ctx_t * tool_ctx ) {
    rc_t rc;
    size_t num_writ;
    bool es = ends_in_slash( tool_ctx -> output_dirname ); /* helper.c */
    if ( fasta_requested( tool_ctx ) ) {
        rc = string_printf( tool_ctx -> dflt_output, sizeof tool_ctx -> dflt_output,
                            &num_writ,
                            es ? "%s%s.fasta" : "%s/%s.fasta",
                            tool_ctx -> output_dirname,
                            tool_ctx -> accession_short );
    } else {
        rc = string_printf( tool_ctx -> dflt_output, sizeof tool_ctx -> dflt_output,
                            &num_writ,
                            es ? "%s%s.fastq" : "%s/%s.fastq",
                            tool_ctx -> output_dirname,
                            tool_ctx -> accession_short );
    }
    if ( 0 != rc ) {
        ErrMsg( "string_printf( output-filename ) -> %R", rc );
    } else {
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
    }
    return rc;
}

static rc_t optionally_create_paths_in_output_filename( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    String path;
    if ( extract_path( tool_ctx -> output_filename, &path ) ) {
        /* the output-filename contains a path... */
        if ( !dir_exists( tool_ctx -> dir, "%S", &path ) ) {
            /* this path does not ( yet ) exist, create it... */
            rc = create_this_dir( tool_ctx -> dir, &path, true );
        }
    }
    return rc;
}

static rc_t adjust_output_filename( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    /* we do have a output-filename : use it */
    if ( dir_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename ) ) { /* helper.c */
        /* the given output-filename is an existing directory ! */
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "string_printf( output-filename ) -> %R", rc );
    } else {
        rc = optionally_create_paths_in_output_filename( tool_ctx );
    }
    return rc;
}

static rc_t adjust_output_filename_by_dir( tool_ctx_t * tool_ctx ) {
    size_t num_writ;
    bool es = ends_in_slash( tool_ctx -> output_dirname ); /* helper.c */
    rc_t rc = string_printf( tool_ctx -> dflt_output, sizeof tool_ctx -> dflt_output,
                        &num_writ,
                        es ? "%s%s" : "%s/%s",
                        tool_ctx -> output_dirname,
                        tool_ctx -> output_filename );
    if ( 0 != rc ) {
        ErrMsg( "string_printf( output-filename ) -> %R", rc );
    } else {
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
        rc = optionally_create_paths_in_output_filename( tool_ctx );
    }
    return rc;
}

static rc_t populate_tool_ctx( tool_ctx_t * tool_ctx, const Args * args ) {
    rc_t rc = ArgsParamValue( args, 0, ( const void ** )&( tool_ctx -> accession_path ) );
    if ( 0 != rc ) {
        ErrMsg( "ArgsParamValue() -> %R", rc );
    }

    if ( 0 == rc ) {
        tool_ctx -> lookup_filename[ 0 ] = 0;
        tool_ctx -> index_filename[ 0 ] = 0;
        tool_ctx -> dflt_output[ 0 ] = 0;

        rc = get_user_input( tool_ctx, args );
        if ( 0 == rc ) {
            encforce_constrains( tool_ctx );
            rc = get_environment( tool_ctx );
        }
        if ( 0 == rc && tool_ctx -> fmt != ft_fasta_us_split_spot ) {
            rc = make_temp_dir( &tool_ctx -> temp_dir,
                            tool_ctx -> requested_temp_path,
                            tool_ctx -> dir );
        }
    }
    
    if ( 0 == rc ) {
        rc = handle_accession( tool_ctx );
    }

    if ( 0 == rc && tool_ctx -> fmt != ft_fasta_us_split_spot ) {
        rc = handle_lookup_path( tool_ctx );
    }

    if ( 0 == rc && NULL != tool_ctx -> output_dirname ) {
        if ( !dir_exists( tool_ctx -> dir, "%s", tool_ctx -> output_dirname ) ) {
            rc = create_this_dir_2( tool_ctx -> dir, tool_ctx -> output_dirname, true );
        }
    }
    
    if ( rc == 0 ) {
        if ( NULL == tool_ctx -> output_filename ) {
            if ( NULL == tool_ctx -> output_dirname ) {
                rc = make_output_filename_from_accession( tool_ctx );
            } else {
                rc = make_output_filename_from_dir_and_accession( tool_ctx );
            }
        } else {
            if ( NULL == tool_ctx -> output_dirname ) {
                rc = adjust_output_filename( tool_ctx );
            } else {
                rc = adjust_output_filename_by_dir( tool_ctx );
            }
        }
    }
    
    tool_ctx -> cleanup_task = NULL;
    if ( tool_ctx -> fmt != ft_fasta_us_split_spot ) {
        if ( 0 == rc ) {
            rc = Make_FastDump_Cleanup_Task ( &( tool_ctx -> cleanup_task ) ); /* cleanup_task.c */
        }

        if ( 0 == rc ) {
            rc = Add_Directory_to_Cleanup_Task ( tool_ctx -> cleanup_task, 
                    get_temp_dir( tool_ctx -> temp_dir ) );
        }
    }

    if ( 0 == rc ) {
        rc = VDBManagerMakeRead( &( tool_ctx -> vdb_mgr ), tool_ctx -> dir );
        if ( 0 != rc ) {
            ErrMsg( "fasterq-dump.c populate_tool_ctx().VDBManagerMakeRead() -> %R\n", rc );
        }
    }
    return rc;
}

static rc_t print_stats( const join_stats_t * stats )
{
    KOutHandlerSetStdErr();
    rc_t rc = KOutMsg( "spots read      : %,lu\n", stats -> spots_read );
    if ( 0 == rc ) {
         rc = KOutMsg( "reads read      : %,lu\n", stats -> reads_read );
    }
    if ( 0 == rc ) {
         rc = KOutMsg( "reads written   : %,lu\n", stats -> reads_written );
    }
    if ( 0 == rc && stats -> reads_zero_length > 0 ) {
         rc = KOutMsg( "reads 0-length  : %,lu\n", stats -> reads_zero_length );
    }
    if ( 0 == rc && stats -> reads_technical > 0 ) {
         rc = KOutMsg( "technical reads : %,lu\n", stats -> reads_technical );
    }
    if ( 0 == rc && stats -> reads_too_short > 0 ) {
         rc = KOutMsg( "reads too short : %,lu\n", stats -> reads_too_short );
    }
    if ( 0 == rc && stats -> reads_invalid > 0 ) {
         rc = KOutMsg( "reads invalid   : %,lu\n", stats -> reads_invalid );
    }
    KOutHandlerSetStdOut();
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

static rc_t produce_lookup_files( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    struct bg_update_t * gap = NULL;
    struct background_file_merger_t * bg_file_merger;
    struct background_vector_merger_t * bg_vec_merger;
    
    if ( tool_ctx -> show_progress ) {
        rc = bg_update_make( &gap, 0 );
    }
   
    /* the background-file-merger catches the files produced by
        the background-vector-merger */
    if ( 0 == rc ) {
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
    }

    /* the background-vector-merger catches the KVectors produced by
       the lookup-produceer */
    if ( 0 == rc ) {
        rc = make_background_vector_merger( &bg_vec_merger,
                 tool_ctx -> dir,
                 tool_ctx -> temp_dir,
                 tool_ctx -> cleanup_task,
                 bg_file_merger,
                 tool_ctx -> num_threads,
                 queue_timeout,
                 tool_ctx -> buf_size,
                 gap ); /* merge_sorter.c */
    }
   
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
    if ( 0 == rc ) {
        rc = execute_lookup_production( tool_ctx -> dir,
                                        tool_ctx -> vdb_mgr,
                                        tool_ctx -> accession_short,
                                        tool_ctx -> accession_path,
                                        bg_vec_merger, /* drives the bg_file_merger */
                                        tool_ctx -> cursor_cache,
                                        tool_ctx -> buf_size,
                                        tool_ctx -> mem_limit,
                                        tool_ctx -> num_threads,
                                        tool_ctx -> show_progress ); /* sorter.c */
    }
    bg_update_start( gap, "merge  : " ); /* progress_thread.c ...start showing the activity... */
            
    if ( 0 == rc ) {
        rc = wait_for_and_release_background_vector_merger( bg_vec_merger ); /* merge_sorter.c */
    }

    if ( 0 == rc ) {
        rc = wait_for_and_release_background_file_merger( bg_file_merger ); /* merge_sorter.c */
    }

    bg_update_release( gap );

    if ( 0 != rc ) {
        ErrMsg( "fasterq-dump.c produce_lookup_files() -> %R", rc );
    }
   
    return rc;
}


/* -------------------------------------------------------------------------------------------- */


static rc_t produce_final_db_output( tool_ctx_t * tool_ctx ) {
    struct temp_registry_t * registry = NULL;
    join_stats_t stats;
    execute_db_join_args_t args;

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

    args . dir = tool_ctx -> dir;
    args . vdb_mgr = tool_ctx -> vdb_mgr;
    args . accession_path = tool_ctx -> accession_path;
    args . accession_short = tool_ctx -> accession_short;
    args . seq_defline = tool_ctx -> seq_defline;
    args . qual_defline = tool_ctx -> qual_defline;
    args . lookup_filename = &( tool_ctx -> lookup_filename[ 0 ] );
    args . index_filename = &( tool_ctx -> index_filename[ 0 ] );
    args . stats = &stats;
    args . join_options = &( tool_ctx -> join_options );
    args . temp_dir = tool_ctx -> temp_dir;
    args . registry= registry;
    args . cursor_cache = tool_ctx -> cursor_cache;
    args . buf_size = tool_ctx -> buf_size;
    args . num_threads = tool_ctx -> num_threads;
    args . row_limit = tool_ctx -> row_limit;
    args . show_progress = tool_ctx -> show_progress;
    args . fmt = tool_ctx -> fmt;

    if ( rc == 0 ) {
        rc = execute_db_join( &args ); /* join.c */
    }

    /* from now on we do not need the lookup-file and it's index any more... */
    if ( 0 != tool_ctx -> lookup_filename[ 0 ] ) {
        KDirectoryRemove( tool_ctx -> dir, true, "%s", &tool_ctx -> lookup_filename[ 0 ] );
    }

    if ( 0 != tool_ctx -> index_filename[ 0 ] ) {
        KDirectoryRemove( tool_ctx -> dir, true, "%s", &tool_ctx -> index_filename[ 0 ] );
    }

    /* STEP 4 : concatenate output-chunks */
    if ( 0 == rc ) {
        if ( tool_ctx -> use_stdout ) {
            rc = temp_registry_to_stdout( registry,
                                          tool_ctx -> dir,
                                          tool_ctx -> buf_size ); /* temp_registry.c */
        } else {
            rc = temp_registry_merge( registry,
                              tool_ctx -> dir,
                              tool_ctx -> output_filename,
                              tool_ctx -> buf_size,
                              tool_ctx -> show_progress,
                              tool_ctx -> force,
                              tool_ctx -> append ); /* temp_registry.c */
        }
    }

    /* in case some of the partial results have not been deleted be the concatenator */
    if ( NULL != registry ) {
        destroy_temp_registry( registry ); /* temp_registry.c */
    }

    if ( 0 == rc ) {
        print_stats( &stats ); /* above */
    }

    return rc;
}

/* -------------------------------------------------------------------------------------------- */

static bool output_exists_whole( tool_ctx_t * tool_ctx ) {
    return file_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename );
}

static bool output_exists_idx( tool_ctx_t * tool_ctx, uint32_t idx ) {
    bool res = false;
    SBuffer_t s_filename;
    rc_t rc = split_filename_insert_idx( &s_filename, 4096,
                            tool_ctx -> output_filename, idx ); /* helper.c */
    if ( 0 == rc ) {
        res = file_exists( tool_ctx -> dir, "%S", &( s_filename . S ) ); /* helper.c */
        release_SBuffer( &s_filename ); /* helper.c */
    }
    return res;
}

static bool output_exists_split( tool_ctx_t * tool_ctx ) {
    bool res = output_exists_whole( tool_ctx );
    if ( !res ) {
        res = output_exists_idx( tool_ctx, 1 );
    }
    if ( !res ) {
        res = output_exists_idx( tool_ctx, 2 );
    }
    return res;
}

static rc_t check_output_exits( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    /* check if the output-file(s) do already exist, in case we are not overwriting */    
    if ( !( tool_ctx -> force ) && !( tool_ctx -> append ) ) {
        bool exists = false;
        switch( tool_ctx -> fmt ) {
            case ft_unknown             : break;
            case ft_fastq_whole_spot    : exists = output_exists_whole( tool_ctx ); break;
            case ft_fastq_split_spot    : exists = output_exists_whole( tool_ctx ); break;
            case ft_fastq_split_file    : exists = output_exists_split( tool_ctx ); break;
            case ft_fastq_split_3       : exists = output_exists_split( tool_ctx ); break;
            case ft_fasta_whole_spot    : exists = output_exists_whole( tool_ctx ); break;
            case ft_fasta_split_spot    : exists = output_exists_whole( tool_ctx ); break;
            case ft_fasta_us_split_spot : exists = output_exists_whole( tool_ctx ); break;
            case ft_fasta_split_file    : exists = output_exists_split( tool_ctx ); break;
            case ft_fasta_split_3       : exists = output_exists_split( tool_ctx ); break;
        }
        if ( exists ) {
            rc = RC( rcExe, rcFile, rcPacking, rcName, rcExists );
            ErrMsg( "fasterq-dump.c fastdump_csra() checking ouput-file '%s' -> %R",
                     tool_ctx -> output_filename, rc );
        }
    }
    return rc;
}

static rc_t process_csra( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    
    if ( ! tool_ctx -> use_stdout ) { rc = check_output_exits( tool_ctx ); } /* above */

    if ( 0 == rc && tool_ctx -> fmt == ft_fasta_us_split_spot ) {
        /* the special case of fasta-unsorted and split-spot : */
        join_stats_t stats;
        execute_unsorted_fasta_db_join_args_t args;

        clear_join_stats( &stats ); /* helper.c */

        args . dir = tool_ctx -> dir;
        args . vdb_mgr = tool_ctx -> vdb_mgr;
        args . accession_short = tool_ctx -> accession_short;
        args . accession_path = tool_ctx -> accession_path;
        args . output_filename = tool_ctx -> use_stdout ? NULL : tool_ctx -> output_filename;
        args . seq_defline = tool_ctx -> seq_defline;
        args . stats = &stats;
        args . join_options = &( tool_ctx -> join_options );
        args . cur_cache = tool_ctx -> cursor_cache;
        args . buf_size = tool_ctx -> buf_size;
        args . num_threads = tool_ctx -> num_threads;
        args . row_limit = tool_ctx -> row_limit;
        args . show_progress = tool_ctx -> show_progress;
        args . force = tool_ctx -> force;
        args . only_unaligned = tool_ctx -> only_unaligned;
        args . only_aligned = tool_ctx -> only_aligned;

        rc = execute_unsorted_fasta_db_join( &args ); /* join.c */

        print_stats( &stats ); /* above */

    } else {
        /* the common case the other cominations of FASTA/FASTQ : */
        if ( 0 == rc ) { rc = produce_lookup_files( tool_ctx ); } /* above */
        if ( 0 == rc ) { rc = produce_final_db_output( tool_ctx ); } /* above */
    }
    return rc;
}


/* -------------------------------------------------------------------------------------------- */

static rc_t process_table( tool_ctx_t * tool_ctx, const char * tbl_name ) {
    rc_t rc = 0;
    join_stats_t stats;
    
    if ( tool_ctx -> only_aligned ) { return rc; }

    clear_join_stats( &stats ); /* helper.c */
    if ( ! tool_ctx -> use_stdout ) { rc = check_output_exits( tool_ctx ); /* above */ }

    if ( 0 == rc && tool_ctx -> fmt == ft_fasta_us_split_spot ) {
        /* this is the 'special' unsorted FASTA for flat tables */
        execute_fasta_tbl_join_args_t args; /* tbl_join.h */
        args . dir = tool_ctx -> dir;
        args . vdb_mgr = tool_ctx -> vdb_mgr;
        args . accession_short = tool_ctx -> accession_short;
        args . accession_path = tool_ctx -> accession_path;
        args . output_filename = tool_ctx -> use_stdout ? NULL : tool_ctx -> output_filename;
        args . seq_defline = tool_ctx -> seq_defline;
        args . tbl_name = tbl_name;
        args . stats = &stats;
        args . join_options = &( tool_ctx -> join_options );
        args . cursor_cache = tool_ctx -> cursor_cache;
        args . buf_size = tool_ctx -> buf_size;
        args . num_threads = tool_ctx -> num_threads;
        args . show_progress = tool_ctx -> show_progress;
        args . force = tool_ctx -> force;
        args . row_limit = tool_ctx -> row_limit;
        rc = execute_unsorted_fasta_tbl_join( &args ); /* tbl_join.c */
    } else {
        /* this is for 'sorted' SPECIAL/FASTQ/FASTA x whole-spot/split-spot/split-file/split-3
           sorted means in the order of the SEQUENCE-table */
        struct temp_registry_t * registry = NULL;
        if ( 0 == rc ) {
            rc = make_temp_registry( &registry, tool_ctx -> cleanup_task ); /* temp_registry.c */
        }

        if ( 0 == rc ) {
            execute_tbl_join_args_t args; /* tbl_join.h */
            args . dir = tool_ctx -> dir;
            args . vdb_mgr = tool_ctx -> vdb_mgr;
            args . accession_short = tool_ctx -> accession_short;
            args . accession_path = tool_ctx -> accession_path;
            args . seq_defline = tool_ctx -> seq_defline;
            args . qual_defline = tool_ctx -> qual_defline;
            args . tbl_name = tbl_name;
            args . stats = &stats;
            args . join_options = &( tool_ctx -> join_options );
            args . temp_dir = tool_ctx -> temp_dir;
            args . registry = registry;
            args . cursor_cache = tool_ctx -> cursor_cache;
            args . buf_size = tool_ctx -> buf_size;
            args . num_threads = tool_ctx -> num_threads;
            args . show_progress = tool_ctx -> show_progress;
            args . fmt = tool_ctx -> fmt;
            args . row_limit = tool_ctx -> row_limit;
            rc = execute_tbl_join( &args ); /* tbl_join.c */
        }

        if ( 0 == rc ) {
            if ( tool_ctx -> use_stdout ) {
                rc = temp_registry_to_stdout( registry,
                                            tool_ctx -> dir,
                                            tool_ctx -> buf_size ); /* temp_registry.c */
            } else {
                rc = temp_registry_merge( registry,
                                tool_ctx -> dir,
                                tool_ctx -> output_filename,
                                tool_ctx -> buf_size,
                                tool_ctx -> show_progress,
                                tool_ctx -> force,
                                tool_ctx -> append ); /* temp_registry.c */
            }
        }
        
        if ( NULL != registry ) {
            destroy_temp_registry( registry ); /* temp_registry.c */
        }
    }
    if ( 0 == rc ) { print_stats( &stats ); /* above */ }
    return rc;
}

/* -------------------------------------------------------------------------------------------- */

static rc_t perform_tool( tool_ctx_t * tool_ctx ) {
    rc_t rc;
    inspector_input_t insp_input;
    inspector_output_t insp_output;

    insp_input . dir = tool_ctx -> dir;
    insp_input . vdb_mgr = tool_ctx -> vdb_mgr;
    insp_input . accession_short = tool_ctx -> accession_short;
    insp_input . accession_path = tool_ctx -> accession_path;
    
    rc = inspect( &insp_input, &insp_output );
    
    if ( 0 == rc ) {
        if ( tool_ctx -> show_details ) {
            rc = show_details( tool_ctx, &insp_output ); /* above */
        }
        /* =================================================== */
        switch( insp_output . acc_type ) {
            /* a cSRA-database with alignments */
            case acc_csra       : rc = process_csra( tool_ctx ); /* above */
                                  break;

            /* a PACBIO-database */
            case acc_pacbio     : ErrMsg( "accession '%s' is PACBIO, please use fastq-dump instead", tool_ctx -> accession_path );
                                  rc = 3; /* signal to main() that the accession is not-processed */
                                  break;

            /* a flat SRA-table */
            case acc_sra_flat   : rc = process_table( tool_ctx, NULL ); /* above */
                                  break;

            /* a cSRA-database withou alignments */
            case acc_sra_db     : rc = process_table( tool_ctx, insp_output . seq_tbl_name ); /* above */
                                  break;

            default             : ErrMsg( "invalid accession '%s'", tool_ctx -> accession_path );
                                  rc = 3; /* signal to main() that the accession is not-found/invalid */
                                  break;
        }
        /* =================================================== */
    } else {
        ErrMsg( "invalid accession '%s'", tool_ctx -> accession_path );
    }
    return rc;
}

/* -------------------------------------------------------------------------------------------- */

rc_t CC KMain ( int argc, char *argv [] ) {
    Args * args;
    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( 0 != rc ) {
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    } else {
        uint32_t param_count;
        rc = ArgsParamCount( args, &param_count );
        if ( 0 != rc ) {
            ErrMsg( "ArgsParamCount() -> %R", rc );
        } else {
            /* in case we are given no or more than one accessions/files to process */
            if ( param_count == 0 || param_count > 1 ) {
                Usage ( args );
                /* will make the caller of this function aka KMane() in man.c return
                error code of 3 */
                rc = 3;
            } else {
                tool_ctx_t tool_ctx;
                rc = populate_tool_ctx( &tool_ctx, args ); /* above */
                if ( 0 == rc ) {
                    rc = perform_tool( &tool_ctx );     /* above */
                    {
                        rc_t rc2 = KDirectoryRelease( tool_ctx . dir );
                        if ( 0 != rc2 ) {
                            ErrMsg( "KDirectoryRelease() -> %R", rc2 );
                            rc = ( 0 == rc ) ? rc2 : rc;
                        }
                    }
                    destroy_temp_dir( tool_ctx . temp_dir ); /* temp_dir.c */
                    {
                        rc_t rc2 = VDBManagerRelease( tool_ctx . vdb_mgr );
                        if ( 0 != rc2 ) {
                            ErrMsg( "VDBManagerRelease() -> %R", rc2 );
                            rc = ( 0 == rc ) ? rc2 : rc;
                        }
                    }
                }
                if ( NULL != tool_ctx . accession_short ) {
                    free( ( char * )tool_ctx . accession_short );
                }
            }
        }
    }
    return rc;
}
