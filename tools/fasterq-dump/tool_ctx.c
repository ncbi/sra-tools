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

#ifndef _h_dflt_defline_
#include "dflt_defline.h"
#endif

static rc_t print_tool_ctx( const tool_ctx_t * tool_ctx ) {
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
        rc = KOutMsg( "scratch-path : '%s'\n", get_temp_dir( tool_ctx -> temp_dir ) /* temp_dir.h */ );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "total ram    : '%lu'\n", tool_ctx -> total_ram );
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
        rc = KOutMsg( "seq-defline  : '%s'\n", tool_ctx -> seq_defline );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "qual-defline  : '%s'\n", tool_ctx -> qual_defline );
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
        rc = KOutMsg( "\n" );
    }
    
    if ( 0 == rc ) {
        rc = inspection_report( &( tool_ctx -> insp_input ), &( tool_ctx -> insp_output ) ); /* inspector.c */
    }
    
    KOutHandlerSetStdOut();
    return rc;
}

static bool output_exists_whole( const tool_ctx_t * tool_ctx ) {
    return file_exists( tool_ctx -> dir, "%s", tool_ctx -> output_filename );
}

static bool output_exists_idx( const tool_ctx_t * tool_ctx, uint32_t idx ) {
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

static bool output_exists_split( const tool_ctx_t * tool_ctx ) {
    bool res = output_exists_whole( tool_ctx );
    if ( !res ) {
        res = output_exists_idx( tool_ctx, 1 );
    }
    if ( !res ) {
        res = output_exists_idx( tool_ctx, 2 );
    }
    return res;
}

static rc_t check_output_exits( const tool_ctx_t * tool_ctx ) {
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

#define DFLT_MAX_FD 32
#define MIN_NUM_THREADS 2
#define MIN_MEM_LIMIT ( 1024L * 1024 * 5 )
#define MAX_BUF_SIZE ( 1024L * 1024 * 1024 )
static void tool_ctx_encforce_constrains( tool_ctx_t * tool_ctx )
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

rc_t release_tool_ctx( const tool_ctx_t * tool_ctx, rc_t rc_in ) {
    rc_t rc = rc_in;
    if ( NULL != tool_ctx -> dir ) {
        rc_t rc2 = KDirectoryRelease( tool_ctx -> dir );
        if ( 0 != rc2 ) {
            ErrMsg( "KDirectoryRelease() -> %R", rc2 );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    destroy_temp_dir( tool_ctx -> temp_dir ); /* temp_dir.c */
    if ( NULL != tool_ctx -> vdb_mgr ) {
        rc_t rc2 = VDBManagerRelease( tool_ctx -> vdb_mgr );
        if ( 0 != rc2 ) {
            ErrMsg( "VDBManagerRelease() -> %R", rc2 );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    if ( NULL != tool_ctx -> accession_short ) {
        free( ( char * )tool_ctx -> accession_short );
    }
    return rc;
}

static rc_t extract_short_accession( tool_ctx_t * tool_ctx ) {
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

/* taken form libs/kapp/main-priv.h */
rc_t KAppGetTotalRam ( uint64_t * totalRam );

rc_t populate_tool_ctx( tool_ctx_t * tool_ctx ) {
    rc_t rc = KDirectoryNativeDir( &( tool_ctx -> dir ) );
    if ( 0 != rc ) {
        ErrMsg( "KDirectoryNativeDir() -> %R", rc );
    }

    if ( 0 == rc ) {
        rc = KAppGetTotalRam ( &( tool_ctx -> total_ram ) );
        if ( 0 != rc ) {
            ErrMsg( "KAppGetTotalRam() -> %R", rc );
        }
    }

    if ( 0 == rc ) {
        tool_ctx_encforce_constrains( tool_ctx ); /* tool_ctx.c */
    }
    
    if ( 0 == rc && tool_ctx -> fmt != ft_fasta_us_split_spot ) {
        rc = make_temp_dir( &tool_ctx -> temp_dir,
                        tool_ctx -> requested_temp_path,
                        tool_ctx -> dir );
    }
    
    if ( 0 == rc ) {
        rc = extract_short_accession( tool_ctx ); /* above */
    }

    if ( 0 == rc && tool_ctx -> fmt != ft_fasta_us_split_spot ) {
        rc = handle_lookup_path( tool_ctx ); /* above */
    }

    if ( 0 == rc && NULL != tool_ctx -> output_dirname ) {
        if ( !dir_exists( tool_ctx -> dir, "%s", tool_ctx -> output_dirname ) ) /* file_tools.c */ {
            rc = create_this_dir_2( tool_ctx -> dir, tool_ctx -> output_dirname, true ); /* file_tools.c */
        }
    }
    
    if ( rc == 0 ) {
        if ( NULL == tool_ctx -> output_filename ) {
            if ( NULL == tool_ctx -> output_dirname ) {
                rc = make_output_filename_from_accession( tool_ctx ); /* above */
            } else {
                rc = make_output_filename_from_dir_and_accession( tool_ctx ); /* above */
            }
        } else {
            if ( NULL == tool_ctx -> output_dirname ) {
                rc = adjust_output_filename( tool_ctx ); /* above */
            } else {
                rc = adjust_output_filename_by_dir( tool_ctx ); /* above */
            }
        }
    }

    if ( ! tool_ctx -> use_stdout ) {
        rc = check_output_exits( tool_ctx ); /* above */
    }

    tool_ctx -> cleanup_task = NULL;
    if ( 0 == rc && tool_ctx -> fmt != ft_fasta_us_split_spot ) {
        rc = Make_FastDump_Cleanup_Task ( &( tool_ctx -> cleanup_task ) ); /* cleanup_task.c */
        if ( 0 == rc ) {
            rc = Add_Directory_to_Cleanup_Task ( tool_ctx -> cleanup_task, 
                    get_temp_dir( tool_ctx -> temp_dir ) ); /* cleanup_task.c */
        }
    }

    if ( 0 == rc ) {
        rc = VDBManagerMakeRead( &( tool_ctx -> vdb_mgr ), tool_ctx -> dir );
        if ( 0 != rc ) {
            ErrMsg( "fasterq-dump.c populate_tool_ctx().VDBManagerMakeRead() -> %R\n", rc );
        }
    }
    
    if ( 0 == rc ) {
        tool_ctx -> insp_input . dir = tool_ctx -> dir;
        tool_ctx -> insp_input . vdb_mgr = tool_ctx -> vdb_mgr;
        tool_ctx -> insp_input . accession_short = tool_ctx -> accession_short;
        tool_ctx -> insp_input . accession_path = tool_ctx -> accession_path;

        rc = inspect( &( tool_ctx -> insp_input ), &( tool_ctx -> insp_output ) ); /* perform the pre-flight inspection: inspector.c */
    }
    
    if ( 0 == rc ) {
        bool fasta = is_format_fasta( tool_ctx -> fmt ); /* helper.c */
        bool use_name = tool_ctx -> insp_output . seq . has_name_column;
        bool use_read_id = false;

        tool_ctx -> seq_defline  = dflt_seq_defline( use_name, use_read_id, fasta );
        tool_ctx -> qual_defline = dflt_qual_defline( use_name, use_read_id );
    }

    if ( 0 == rc && tool_ctx -> show_details ) {
        rc = print_tool_ctx( tool_ctx ); /* above */
    }

    return rc;
}
