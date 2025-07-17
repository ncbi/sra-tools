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

#include "tool_ctx.h"

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_temp_dir_
#include "temp_dir.h"
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

#ifndef _h_arg_helper_
#include "arg_helper.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_inspector_
#include "inspector.h"
#endif

#ifndef _h_cleanup_task_
#include "cleanup_task.h"
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_klib_namelist_
#include <klib/namelist.h>
#endif

#include <klib/report.h> /* ReportResetObject */
#include <vdb/report.h> /* ReportSetVDBManager */

#ifndef _h_kproc_task_
#include <kproc/task.h>
#endif

#ifndef _h_dflt_defline_
#include "dflt_defline.h"
#endif

bool tctx_populate_cmn_iter_params( const tool_ctx_t * tool_ctx,
                                        cmn_iter_params_t * params ) {
    bool res = false;
    if ( NULL != tool_ctx && NULL != params ) {
        res = cmn_iter_populate_params( params,
                tool_ctx -> dir,
                tool_ctx -> vdb_mgr,
                tool_ctx -> accession_short,
                tool_ctx -> accession_path,
                tool_ctx -> cursor_cache,
                0,
                0 ); /* in cmn_iter.c */
    }
    return res;
}

static rc_t tctx_print( const tool_ctx_t * tool_ctx ) {
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
        rc = KOutMsg( "row-limit    : %,lu rows\n", tool_ctx -> row_limit );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "scratch-path : '%s'\n", get_temp_dir( tool_ctx -> temp_dir ) /* temp_dir.h */ );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "total ram    : %,lu bytes\n", tool_ctx -> total_ram );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "output-format: %s\n", hlp_fmt_2_string( tool_ctx -> fmt ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "check-mode   : %s\n", hlp_check_mode_2_string( tool_ctx -> check_mode ) );
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
        rc = KOutMsg( "output       : '%s'\n", tool_ctx -> dflt_output ); /* cannot be NULL */
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "append-mode  : '%s'\n", hlp_yes_or_no( tool_ctx -> append ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "stdout-mode  : '%s'\n", hlp_yes_or_no( tool_ctx -> use_stdout ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "seq-defline  : '%s'\n", tool_ctx -> seq_defline );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "qual-defline  : '%s'\n", tool_ctx -> qual_defline );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "only-unaligned : '%s'\n", hlp_yes_or_no( tool_ctx -> only_unaligned ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "only-aligned   : '%s'\n", hlp_yes_or_no( tool_ctx -> only_aligned ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "accession     : '%s'\n", tool_ctx -> accession_short );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "accession-path: '%s'\n", tool_ctx -> accession_path );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "est. output          : %,lu bytes\n", tool_ctx -> estimated_output_size );
    }
    if ( 0 == rc && tool_ctx -> disk_limit_out_cmdl > 0 ) {
        rc = KOutMsg( "disk-limit input     : %,lu bytes\n", tool_ctx -> disk_limit_out_cmdl );
    }
    if ( 0 == rc && tool_ctx -> disk_limit_tmp_cmdl > 0 ) {
        rc = KOutMsg( "disk-limit-tmp input : %,lu bytes\n", tool_ctx -> disk_limit_tmp_cmdl );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "disk-limit (OS)      : %,lu bytes\n", tool_ctx -> disk_limit_out_os );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "disk-limit-tmp (OS)  : %,lu bytes\n", tool_ctx -> disk_limit_tmp_os );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "out/tmp on same fs   : '%s'\n", hlp_yes_or_no( tool_ctx -> out_and_tmp_on_same_fs ) );
    }
    if ( ft_fasta_ref_tbl == tool_ctx -> fmt ) {
        if ( 0 == rc ) {        
            rc = KOutMsg( "only internal ref    : '%s'\n", hlp_yes_or_no( tool_ctx -> only_internal_refs ) );
        }
        if ( 0 == rc ) {
            rc = KOutMsg( "only external ref    : '%s'\n", hlp_yes_or_no( tool_ctx -> only_external_refs ) );
        }
        if ( 0 == rc ) {
            rc = KOutMsg( "use name             : '%s'\n", hlp_yes_or_no( tool_ctx -> use_name ) );
        }
        if ( 0 == rc ) {
            uint32_t idx, count = 0;
            rc = VNameListCount( tool_ctx -> ref_name_filter, &count );
            for ( idx = 0; 0 == rc && idx < count; ++idx ) {
                const char * name = NULL;
                rc = VNameListGet( tool_ctx -> ref_name_filter, idx, &name );
                if ( 0 == rc && NULL != name ) {
                    rc = KOutMsg( "\tref[%u] : '%s'\n", idx, name );
                }
            }
        }
    }

    if ( 0 == rc ) {
        rc = KOutMsg( "\n" );
    }
    if ( 0 == rc ) {
        rc = insp_report( &( tool_ctx -> insp_input ), &( tool_ctx -> insp_output ) );
    }
    KOutHandlerSetStdOut();
    return rc;
}

static bool tctx_output_exists_whole( const tool_ctx_t * tool_ctx ) {
    bool res = ft_file_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename );
    if ( !res ) {
        res = ft_dir_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename );
    }
    return res;
}

static bool tctx_output_exists_idx( const tool_ctx_t * tool_ctx, uint32_t idx ) {
    bool res = false;
    SBuffer_t s_filename;

    rc_t rc = split_filename_insert_idx( &s_filename, 4096,
                            tool_ctx -> output_filename, idx ); /* sbuffer.c */
    if ( 0 == rc ) {
        res = ft_file_exists( tool_ctx -> dir, "%S", &( s_filename . S ) );
        if ( !res ) {
            res = ft_dir_exists( tool_ctx -> dir, "%S", &( s_filename . S ) );
        }
        release_SBuffer( &s_filename ); /* helper.c */
    }
    return res;
}

static bool tctx_output_exists_split( const tool_ctx_t * tool_ctx ) {
    bool res = tctx_output_exists_whole( tool_ctx );
    if ( !res ) {
        res = tctx_output_exists_idx( tool_ctx, 1 );
    }
    if ( !res ) {
        res = tctx_output_exists_idx( tool_ctx, 2 );
    }
    return res;
}

static rc_t tctx_check_output_exits( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    /* check if the output-file(s) do already exist, in case we are not overwriting */
    if ( !( tool_ctx -> force ) && !( tool_ctx -> append ) ) {
        bool exists = false;
        switch( tool_ctx -> fmt ) {
            case ft_fastq_whole_spot    : exists = tctx_output_exists_whole( tool_ctx ); break;
            case ft_fastq_split_spot    : exists = tctx_output_exists_whole( tool_ctx ); break;
            case ft_fastq_split_file    : exists = tctx_output_exists_split( tool_ctx ); break;
            case ft_fastq_split_3       : exists = tctx_output_exists_split( tool_ctx ); break;
            case ft_fasta_whole_spot    : exists = tctx_output_exists_whole( tool_ctx ); break;
            case ft_fasta_split_spot    : exists = tctx_output_exists_whole( tool_ctx ); break;
            case ft_fasta_us_split_spot : exists = tctx_output_exists_whole( tool_ctx ); break;
            case ft_fasta_split_file    : exists = tctx_output_exists_split( tool_ctx ); break;
            case ft_fasta_split_3       : exists = tctx_output_exists_split( tool_ctx ); break;
            default : break;
        }
        if ( exists ) {
            tool_ctx -> force = true;
        }
    }
    return rc;
}

#define DFLT_MAX_FD 32
#define MIN_NUM_THREADS 2
#define MIN_MEM_LIMIT ( 1024L * 1024 * 5 )
#define MAX_BUF_SIZE ( 1024L * 1024 * 1024 )
static rc_t tctx_encforce_constrains( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    bool ignore_stdout = false;
    uint32_t env_thread_count = ahlp_get_env_u32( "DLFT_THREAD_COUNT", 0 );
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
            case ft_fastq_whole_spot    : break;
            case ft_fastq_split_spot    : break;
            case ft_fastq_split_file    : tool_ctx -> use_stdout = false; ignore_stdout = true; break;
            case ft_fastq_split_3       : tool_ctx -> use_stdout = false; ignore_stdout = true; break;

            case ft_fasta_whole_spot    : break;
            case ft_fasta_split_spot    : break;
            case ft_fasta_us_split_spot : break;
            case ft_fasta_split_file    : tool_ctx -> use_stdout = false; ignore_stdout = true; break;
            case ft_fasta_split_3       : tool_ctx -> use_stdout = false; ignore_stdout = true; break;
            
            default : break;
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
    if ( ignore_stdout ) {
        rc = RC( rcExe, rcFile, rcPacking, rcName, rcExists );
        ErrMsg( "directing output to stdout requested." );
        ErrMsg( "but requested mode ( %s ) would produce multiple files", hlp_fmt_2_string( tool_ctx -> fmt ) );
    }
    return rc;
}

rc_t tctx_release( const tool_ctx_t * tool_ctx, rc_t rc_in ) {
    rc_t rc = rc_in;
	
	KTaskRelease( ( KTask* )tool_ctx -> cleanup_task );
		
    if ( NULL != tool_ctx -> dir ) {
        rc_t rc2 = KDirectoryRelease( tool_ctx -> dir );
        if ( 0 != rc2 ) {
            ErrMsg( "release_tool_ctx . KDirectoryRelease() -> %R", rc2 );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }

    if ( !tool_ctx -> keep_tmp_files ) {
        destroy_temp_dir( tool_ctx -> temp_dir ); /* temp_dir.c */
    }

    if ( NULL != tool_ctx -> vdb_mgr ) {
        rc_t rc2 = VDBManagerRelease( tool_ctx -> vdb_mgr );
        if ( 0 != rc2 ) {
            ErrMsg( "release_tool_ctx . VDBManagerRelease() -> %R", rc2 );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    if ( NULL != tool_ctx -> accession_short ) {
        free( ( char * )tool_ctx -> accession_short );
    }
    if ( NULL != tool_ctx -> ref_name_filter ) {
        rc_t rc2 = VNamelistRelease( tool_ctx -> ref_name_filter );
        if ( 0 != rc2 ) {
            ErrMsg( "release_tool_ctx . VNamelistRelease() -> %R", rc2 );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}

static rc_t tctx_extract_short_accession( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    tool_ctx -> accession_short = insp_extract_acc_from_path( tool_ctx -> accession_path ); /* inspector.c */
    ReportResetObject(  tool_ctx -> accession_path );
    ReportBuildDate( __DATE__ );
    ReportSetVDBManager( tool_ctx -> vdb_mgr );

    if ( NULL == tool_ctx -> accession_short ) {
        rc = RC( rcApp, rcArgv, rcAccessing, rcParam, rcInvalid );
        ErrMsg( "accession '%s' invalid", tool_ctx -> accession_path );
    }
    return rc;
}

static rc_t tctx_create_lookup_and_index_path( tool_ctx_t * tool_ctx ) {
    rc_t rc = generate_lookup_filename( tool_ctx -> temp_dir,
                                        &tool_ctx -> lookup_filename[ 0 ],
                                        sizeof tool_ctx -> lookup_filename ); /* temp_dir.c */
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

/* we have NO output-dir and NO output-file */
static rc_t tctx_make_output_filename_from_accession( tool_ctx_t * tool_ctx, bool fasta ) {
    /* we DO NOT have a output-directory : build output-filename from the accession */
    /* generate the full path of the output-file, if not given */
    rc_t rc = KDirectoryResolvePath( tool_ctx -> dir,
                                true /* absolute */,
                                &( tool_ctx -> dflt_output[ 0 ] ),
                                sizeof tool_ctx -> dflt_output,
                                "%s%s",
                                tool_ctx -> accession_short,
                                hlp_out_ext( fasta ) /* helper.c */ );
    if ( 0 != rc ) {
        ErrMsg( "tool_ctx_make_output_filename_from_accession.KDirectoryResolvePath() -> %R", rc );
    } else {
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
    }
    return rc;
}

/* we have an output-dir and NO output-file */
static rc_t tctx_make_output_filename_from_dir_and_accession( tool_ctx_t * tool_ctx, bool fasta ) {
    bool es = hlp_ends_in_slash( tool_ctx -> output_dirname ); /* helper.c */
    rc_t rc = KDirectoryResolvePath( tool_ctx -> dir,
                                true /* absolute */,
                                &( tool_ctx -> dflt_output[ 0 ] ),
                                sizeof tool_ctx -> dflt_output,
                                es ? "%s%s%s" : "%s/%s%s",
                                tool_ctx -> output_dirname,
                                tool_ctx -> accession_short,
                                hlp_out_ext( fasta ) /* helper.c */ );
    if ( 0 != rc ) {
        ErrMsg( "tool_ctx_make_output_filename_from_dir_and_accession.KDirectoryResolvePath() -> %R", rc );
    } else {
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
    }
    return rc;
}

static rc_t tctx_optionally_create_paths_in_output_filename( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    String path;
    if ( hlp_extract_path( tool_ctx -> output_filename, &path ) ) {
        /* the output-filename contains a path... */
        if ( !ft_dir_exists( tool_ctx -> dir, "%S", &path ) ) {
            /* this path does not ( yet ) exist, create it... */
            rc = ft_create_this_dir( tool_ctx -> dir, &path, true );
        }
    }
    return rc;
}

static rc_t tctx_resolve_output_filename( tool_ctx_t * tool_ctx ) {
    rc_t rc = KDirectoryResolvePath( tool_ctx -> dir,
                                true /* absolute */,
                                &( tool_ctx -> dflt_output[ 0 ] ),
                                sizeof tool_ctx -> dflt_output,
                                "%s",
                                tool_ctx -> output_filename );
    if ( 0 != rc ) {
        ErrMsg( "tool_ctx_resolve_output_filename.KDirectoryResolvePath() -> %R", rc );
    } else {
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
    }
    return rc;
}

static rc_t tctx_adjust_output_filename( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    /* we do have a output-filename : use it */
    if ( ft_dir_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename ) ) { /* helper.c */
        /* the given output-filename is an existing directory ! */
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "output >%s< already exists", tool_ctx -> output_filename );
    } else {
        rc = tctx_optionally_create_paths_in_output_filename( tool_ctx ); /* above */
    }
    return rc;
}

static rc_t tctx_adjust_output_filename_by_dir( tool_ctx_t * tool_ctx ) {
    bool es = hlp_ends_in_slash( tool_ctx -> output_dirname ); /* helper.c */
    rc_t rc = KDirectoryResolvePath( tool_ctx -> dir,
                                true /* absolute */,
                                &( tool_ctx -> dflt_output[ 0 ] ),
                                sizeof tool_ctx -> dflt_output,
                                es ? "%s%s" : "%s/%s",
                                tool_ctx -> output_dirname,
                                tool_ctx -> output_filename );
    if ( 0 != rc ) {
        ErrMsg( "tool_ctx_adjust_output_filename_by_dir.KDirectoryResolvePath() -> %R", rc );
    } else {
        tool_ctx -> output_filename = tool_ctx -> dflt_output;
        rc = tctx_optionally_create_paths_in_output_filename( tool_ctx );
    }
    return rc;
}

static void tctx_get_disk_limits( tool_ctx_t * tool_ctx ) {
    /* we do not stop processing if there is an error while asking for this value */
    ft_available_space_disk_space( tool_ctx -> dir,
                                tool_ctx -> output_filename,
                                &( tool_ctx -> disk_limit_out_os ),
                                true /* is_file */ ); /* file_tools.c */
    if ( NULL != tool_ctx -> temp_dir ) {
        ft_available_space_disk_space( tool_ctx -> dir,
                                    get_temp_dir( tool_ctx -> temp_dir ), /* tmp_dir.c */
                                    &( tool_ctx -> disk_limit_tmp_os ),
                                    false /* is_file */ ); /* file_tools.c */
    }
}

static size_t tctx_temp_file_sizes( tool_ctx_t * tool_ctx ) {
    size_t res = 0;

    /* if the accession is not local, each thread creates it's own local cache of it */
    if ( tool_ctx -> insp_output . is_remote ) {
        res = ( tool_ctx -> insp_output . acc_size * tool_ctx -> num_threads );
    }

    /* in case of ft_fasta_us_split_spot: there are no temp-files*/
    if ( ft_fasta_us_split_spot != tool_ctx -> fmt ) {
        /* if we do use temp-files: they need as much space as the generated output */
        res = tool_ctx -> estimated_output_size;
        /* plus ( size / thread-count ) */
        res += ( tool_ctx -> estimated_output_size / tool_ctx -> num_threads );
    }

    return res;
}

static size_t tool_ctx_get_temp_file_limit( tool_ctx_t * tool_ctx ) {
    size_t res = tool_ctx -> disk_limit_tmp_cmdl;   /* the commandline has priority! */
    if ( 0 == res ) { res = tool_ctx -> disk_limit_out_cmdl; } /* if not specific tmp-limit given */
    if ( 0 == res ) { res = tool_ctx -> disk_limit_tmp_os; } /* what the OS thinks is free */
    if ( 0 == res ) { res = tool_ctx -> disk_limit_out_os; } /* as a last resort, if above has not value */
    return res;
}

static size_t tctx_get_out_file_limit( tool_ctx_t * tool_ctx ) {
    size_t res = tool_ctx -> disk_limit_out_cmdl;   /* the commandline has priority! */
    if ( 0 == res ) { res = tool_ctx -> disk_limit_out_os; } /* as a last resort, if above has not value */
    return res;
}

static rc_t tctx_check_available_disk_size( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;

    if ( tool_ctx -> use_stdout ) {
        /* if the tool is set to output on stdout: we only need to check the temp-file-size */
        size_t needed = tctx_temp_file_sizes( tool_ctx ); /* above */
        /* the limit is either given at the command-line or queried from the OS */
        size_t limit = tool_ctx_get_temp_file_limit( tool_ctx );
        if ( limit > 0 && needed > limit ) {
            /* we are over the limit! */
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcTooBig );
        }
    } else {
        /* we have to check for output- as well as for temp-file-size */
        if ( tool_ctx -> out_and_tmp_on_same_fs ) {
            /* output as well as temp-files are on the same file-system, temp-size includes the output */
            size_t needed_tmp = tctx_temp_file_sizes( tool_ctx ); /* above */
            size_t needed_out = tool_ctx -> estimated_output_size;
            size_t needed = needed_tmp > needed_out ? needed_tmp : needed_out;
            size_t limit = tctx_get_out_file_limit( tool_ctx );
            if ( 0 == limit ) { limit = tool_ctx_get_temp_file_limit( tool_ctx ); }
            if ( limit > 0 && needed > limit ) {
                /* we are over the limit! */
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcTooBig );
            }
        } else {
            /* output and temp-files are on different file-systems */
            size_t needed_tmp = tctx_temp_file_sizes( tool_ctx ); /* above */
            size_t needed_out = tool_ctx -> estimated_output_size;
            size_t needed = needed_tmp > needed_out ? needed_tmp : needed_out;
            size_t limit = tctx_get_out_file_limit( tool_ctx ); /* above */
            if ( limit > 0 && needed > limit ) {
                /* we are over the limit! */
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcTooBig );
            } else {
                limit = tool_ctx_get_temp_file_limit( tool_ctx );
                if ( limit > 0 && needed > limit ) {
                    /* we are over the limit! */
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcTooBig );
                }
            }
        }
    }

    if ( 0 == rc ) {
        if ( tool_ctx -> show_details ) {
            rc_t rc2 = KOutHandlerSetStdErr();
            if ( 0 == rc2 ) {
                rc2 = KOutMsg( "disk-limit(s) not exeeded!\n" );
                KOutHandlerSetStdOut();
            }
        }
    } else {
        rc_t rc2 = KOutHandlerSetStdErr();
        if ( 0 == rc2 ) {
            rc2 = KOutMsg( "disk-limit exeeded!\n" );
            if ( 0 == rc2 && !tool_ctx -> show_details ) {
                rc2 = KOutMsg( "to see limits: re-run with '-x' option.\n" );
            }
            KOutHandlerSetStdOut();
        }
    }
    return rc;
}

static bool tctx_does_format_produce_reads_in_a_single_file( format_t fmt ) {
    switch ( fmt ) {
        case ft_unknown             : return false; break;
        case ft_fastq_whole_spot    : return false; break;
        case ft_fastq_split_spot    : return true; break;
        case ft_fastq_split_file    : return false; break;
        case ft_fastq_split_3       : return false; break;
        case ft_fasta_whole_spot    : return false; break;
        case ft_fasta_split_spot    : return true; break;
        case ft_fasta_split_file    : return false; break;
        case ft_fasta_split_3       : return false; break;
        case ft_fasta_us_split_spot : return true; break;
        case ft_fasta_ref_tbl       : return true; break;
        case ft_fasta_concat        : return true; break;        
        case ft_ref_report          : return true; break;
    }
    return false;
}


static rc_t tctx_check_available_columns( tool_ctx_t * tool_ctx ) {
    rc_t rc = 0;
    
    if ( !hlp_is_format_fasta( tool_ctx -> fmt ) ) {
        /* FASTQ is requested... */
        if ( !tool_ctx -> insp_output . seq . has_quality_column ) {
            rc = RC( rcApp, rcArgv, rcAccessing, rcParam, rcInvalid );
            ErrMsg( "the input data is missing the QUALITY-column" );
        }
    }
    if ( 0 == rc ) {
        /* we need READ in any case... */
        if ( !tool_ctx -> insp_output . seq . has_read_column ) {
            rc = RC( rcApp, rcArgv, rcAccessing, rcParam, rcInvalid );
            ErrMsg( "the input data is missing the READ-column" );
        }
    }
    return rc;
}

/* taken form libs/kapp/main-priv.h */
rc_t KAppGetTotalRam ( uint64_t * totalRam );

rc_t tctx_populate_and_call_inspector( tool_ctx_t * tool_ctx ) {

    bool fasta = hlp_is_format_fasta( tool_ctx -> fmt ); /* helper.c */

    /* create the KDirectory-instance for all modules to use */
    rc_t rc = KDirectoryNativeDir( &( tool_ctx -> dir ) );
    if ( 0 != rc ) {
        ErrMsg( "KDirectoryNativeDir() -> %R", rc );
    }

    /* create the VDB-Manager-instance for all modules to use */
    if ( 0 == rc ) {
        rc = VDBManagerMakeRead( &( tool_ctx -> vdb_mgr ), tool_ctx -> dir );
        if ( 0 != rc ) {
            ErrMsg( "fasterq-dump.c populate_tool_ctx().VDBManagerMakeRead() -> %R\n", rc );
        }
    }

    /* query the amount of RAM available */
    if ( 0 == rc ) {
        rc = KAppGetTotalRam ( &( tool_ctx -> total_ram ) ); /* kapp-library */
        if ( 0 != rc ) {
            ErrMsg( "KAppGetTotalRam() -> %R", rc );
        }
    }

    /* enforce some constrains: thread-count, mem-limit, buffer-size, stdout, only-aligned/unaligned */
    if ( 0 == rc ) {
        rc = tctx_encforce_constrains( tool_ctx );
    }

    /* extract the accesion-string, for output- and temp-files to use */
    if ( 0 == rc ) {
        rc = tctx_extract_short_accession( tool_ctx );
    }

    /* create the temp. directory ( only if we need it! ) */
    if ( 0 == rc && tool_ctx -> fmt != ft_fasta_us_split_spot ) {
        rc = make_temp_dir( &tool_ctx -> temp_dir,
                        tool_ctx -> requested_temp_path,
                        tool_ctx -> dir );
    }

    /* create the lookup- and index-filenames ( only if we need it! ) */
    if ( 0 == rc && tool_ctx -> fmt != ft_fasta_us_split_spot ) {
        rc = tctx_create_lookup_and_index_path( tool_ctx );
    }

    /* if an output-directory is explicity given from the commandline: create if not exists */
    if ( 0 == rc && NULL != tool_ctx -> output_dirname && cmt_only != tool_ctx -> check_mode ) {
        if ( !ft_dir_exists( tool_ctx -> dir, "%s", tool_ctx -> output_dirname ) ) {
            rc = ft_create_this_dir_2( tool_ctx -> dir, tool_ctx -> output_dirname, true );
        }
    }

    /* create of adjust the output-filename(s) if needed */
    if ( rc == 0 && !tool_ctx -> use_stdout ) {
        if ( NULL == tool_ctx -> output_filename ) {
            /* no output-filename has been given on the commandline */
            if ( NULL == tool_ctx -> output_dirname ) {
                rc = tctx_make_output_filename_from_accession( tool_ctx, fasta );
            } else {
                rc = tctx_make_output_filename_from_dir_and_accession( tool_ctx, fasta );
            }
        } else {
            /* there is an output-filename on the commandline */
            if ( NULL == tool_ctx -> output_dirname ) {
                rc = tctx_resolve_output_filename( tool_ctx );
                if ( 0 == rc ) {
                    rc = tctx_adjust_output_filename( tool_ctx );
                }
            } else {
                rc = tctx_adjust_output_filename_by_dir( tool_ctx );
            }
        }
        if ( 0 == rc && cmt_only != tool_ctx -> check_mode ) {
            rc = tctx_check_output_exits( tool_ctx );
        }
    }

    /* create the cleanup-taks ( for modules to add file/directories to it ) and add the tem-dir to it */
    tool_ctx -> cleanup_task = NULL;
    if ( 0 == rc && tool_ctx -> fmt != ft_fasta_us_split_spot ) {
        rc = clt_create( &( tool_ctx -> cleanup_task ),
                         tool_ctx -> show_details,
                         tool_ctx -> keep_tmp_files ); /* cleanup_task.c */
        if ( 0 == rc ) {
            rc = clt_add_directory( tool_ctx -> cleanup_task, 
                    get_temp_dir( tool_ctx -> temp_dir ) ); /* cleanup_task.c */
        }
    }

    /* run the inspector to extract valuable info about the accession */
    if ( 0 == rc ) {
        tool_ctx -> insp_input . dir = tool_ctx -> dir;
        tool_ctx -> insp_input . vdb_mgr = tool_ctx -> vdb_mgr;
        tool_ctx -> insp_input . accession_short = tool_ctx -> accession_short;
        tool_ctx -> insp_input . accession_path = tool_ctx -> accession_path;
        tool_ctx -> insp_input . requested_seq_tbl_name = tool_ctx -> requested_seq_tbl_name;
        
        rc = inspect( &( tool_ctx -> insp_input ), &( tool_ctx -> insp_output ) ); /* inspector.c */
    }

    /* check for presence of columns for certain output-types FASTA vs. FASTQ */
    if ( 0 == rc ) {
        rc = tctx_check_available_columns( tool_ctx );
    }
    
    /* create seq/qual deflines ( if they are not given at the commandline ) */
    if ( 0 == rc ) {
        bool has_name = tool_ctx -> insp_output . seq . has_name_column;
        bool use_name = true;

        /* use_read_id should be true if the output is a single file */
        bool use_read_id = tctx_does_format_produce_reads_in_a_single_file( tool_ctx -> fmt );

        if ( NULL == tool_ctx -> seq_defline ) {
            tool_ctx -> seq_defline  = dflt_seq_defline( has_name, use_name, use_read_id, fasta ); /* dflt_defline.c */
        }
        if ( NULL == tool_ctx -> qual_defline && !fasta ) {
            tool_ctx -> qual_defline = dflt_qual_defline( has_name, use_name, use_read_id ); /* dflt_defline.c */
        }
    }

    /* special check if we have a combination of split-3 and include-technical */
    if ( 0 == rc ) {
        bool split_3_requested = ( ft_fasta_split_3 == tool_ctx -> fmt || ft_fastq_split_3 == tool_ctx -> fmt );
        bool include_technical = !( tool_ctx -> join_options . skip_tech );
        if ( split_3_requested && include_technical ) {
            /* warn the user, and switch to split-file-mode */
            StdErrMsg( "split-3-mode cannot be combined with include-technical -> switching to split-file-mode\n" );
            switch ( tool_ctx -> fmt ) {
                case ft_fasta_split_3 : tool_ctx -> fmt = ft_fasta_split_file; break;
                case ft_fastq_split_3 : tool_ctx -> fmt = ft_fastq_split_file; break;
                default : ;
            }
        }
    }
        
    /* evaluate the free-disk-space according the os */
    if ( 0 == rc ) { tctx_get_disk_limits( tool_ctx ); }

    /* create an estimation of the output-size */
    if ( 0 == rc && hlp_is_perform_check( tool_ctx -> check_mode ) ) {
        insp_estimate_input_t iei;

        iei . insp = &( tool_ctx -> insp_output );
        iei . seq_defline  = tool_ctx -> seq_defline;
        iei . qual_defline = tool_ctx -> qual_defline;
        iei . acc = tool_ctx -> accession_short;
        iei . avg_name_len = tool_ctx -> insp_output . seq . avg_name_len;
        iei . avg_bio_reads = tool_ctx -> insp_output . seq . avg_bio_reads;
        iei . avg_tech_reads = tool_ctx -> insp_output . seq . avg_tech_reads;
        iei . skip_tech = tool_ctx -> join_options . skip_tech;
        iei . fmt = tool_ctx -> fmt;

        tool_ctx -> estimated_output_size = insp_estimate_output_size( &iei );
    }

    /* determine if output and temp. path are on the same file-systme ( work is in helper-function ) */
    if ( 0 == rc ) {
        tool_ctx -> out_and_tmp_on_same_fs = hlp_paths_on_same_filesystem(
            tool_ctx -> output_filename, get_temp_dir( tool_ctx -> temp_dir ) );
    }

    /* print all the values gathered here, if requested */
    if ( 0 == rc && tool_ctx -> show_details ) {
        rc = tctx_print( tool_ctx );
    }

    /* check if we have enough space, based on the check-mode */
    if ( 0 == rc ) {
        switch( tool_ctx -> check_mode ) { /* check-mode defined in helper.h */
            case cmt_on     :
            case cmt_only   : rc = tctx_check_available_disk_size( tool_ctx ); break;
            default         : break;
        }
    }
    return rc;
}
