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

#ifndef _h_out_redir_
#include "out_redir.h"
#endif

#ifndef _h_dyn_string_
#include "dyn_string.h"
#endif

#ifndef _h_reref_
#include "reref.h"
#endif

#ifndef _h_report_deletes_
#include "report_deletes.h"
#endif

#ifndef _h_walk_debug_
#include "walk_debug.h"
#endif

#ifndef _h_4na_ascii_
#include "4na_ascii.h"
#endif

#ifndef _h_pileup_counters_
#include "pileup_counters.h"
#endif

#ifndef _h_pileup_index_
#include "pileup_index.h"
#endif

#ifndef _h_pileup_varcount_
#include "pileup_varcount.h"
#endif

#ifndef _h_pileup_indels_
#include "pileup_indels.h"
#endif

#ifndef _h_pileup_stat_
#include "pileup_stat.h"
#endif

#ifndef _h_pileup_v2_
#include "pileup_v2.h"
#endif

#ifndef _h_kapp_main_
#include <kapp/main.h>
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif

#ifndef _h_klib_report_
#include <klib/report.h>
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

#ifndef _h_kdb_manager_
#include <kdb/manager.h>  /* kptDatabase */
#endif

#ifndef _h_kfg_config_
#include <kfg/config.h> /* KConfigSetNgcFile */
#endif

#ifndef _h_vdb_report_
#include <vdb/report.h> /* ReportSetVDBManager() */
#endif

#ifndef _h_vdb_vdb_priv_
#include <vdb/vdb-priv.h> /* VDBManagerDisablePagemapThread() */
#endif

#ifndef _h_sra_sraschema_
#include <sra/sraschema.h>
#endif

#ifndef _h_align_manager_
#include <align/manager.h>
#endif

#include <stdio.h>  /* because of fwrite() */

#define COL_QUALITY "QUALITY"
#define COL_REF_ORIENTATION "REF_ORIENTATION"
#define COL_READ_FILTER "READ_FILTER"
#define COL_TEMPLATE_LEN "TEMPLATE_LEN"

#define OPTION_MINMAPQ "minmapq"
#define ALIAS_MINMAPQ  "q"

#define OPTION_DUPS    "duplicates"
#define ALIAS_DUPS     "d"

#define OPTION_NOSKIP  "noskip"
#define ALIAS_NOSKIP   "s"

#define OPTION_SHOWID  "showid"
#define ALIAS_SHOWID   "i"

#define OPTION_SPOTGRP "spotgroups"
#define ALIAS_SPOTGRP  "p"

#define OPTION_SEQNAME "seqname"
#define ALIAS_SEQNAME  "e"

#define OPTION_MIN_M   "minmismatch"
#define OPTION_MERGE   "merge-dist"

#define OPTION_DEPTH_PER_SPOTGRP	"depth-per-spotgroup"

#define OPTION_NGC "ngc"

#define OPTION_FUNC    "function"
#define ALIAS_FUNC     NULL

#define FUNC_COUNTERS   "count"
#define FUNC_STAT       "stat"
#define FUNC_RE_REF     "ref"
#define FUNC_RE_REF_EXT "ref-ex"
#define FUNC_DEBUG      "debug"
#define FUNC_MISMATCH   "mismatch"
#define FUNC_INDEX      "index"
#define FUNC_TEST       "test"
#define FUNC_VARCOUNT   "varcount"
#define FUNC_DELETES    "deletes"
#define FUNC_INDELS     "indels"

enum {
    sra_pileup_samtools = 0,
    sra_pileup_counters = 1,
    sra_pileup_stat = 2,
    sra_pileup_report_ref = 3,
    sra_pileup_report_ref_ext = 4,
    sra_pileup_debug = 5,
    sra_pileup_mismatch = 6,
    sra_pileup_index = 7,
    sra_pileup_test = 8,
    sra_pileup_varcount = 9,
    sra_pileup_deletes = 10,
	sra_pileup_indels = 11
};

static const char * minmapq_usage[]         = { "Minimum mapq-value, ",
                                                "alignments with lower mapq",
                                                "will be ignored (default=0)", NULL };

static const char * dups_usage[]            = { "process duplicates 0..off/1..on", NULL };

static const char * noskip_usage[]          = { "Does not skip reference-regions without alignments", NULL };

static const char * showid_usage[]          = { "Shows alignment-id for every base", NULL };

static const char * spotgrp_usage[]         = { "divide by spotgroups", NULL };

static const char * dpgrp_usage[]         	= { "print depth per spotgroup", NULL };

static const char * seqname_usage[]         = { "use original seq-name", NULL };

static const char * min_m_usage[]           = { "min percent of mismatches used in function mismatch, default is 5%", NULL };

static const char * merge_usage[]           = { "If adjacent slices are closer than this, ",
                                                "they are merged and a skiplist is created. ",
                                                "a value of zero disables the feature, default is 10000", NULL };

static const char * func_ref_usage[]        = { "list references", NULL };
static const char * func_ref_ex_usage[]     = { "list references + coverage", NULL };
static const char * func_count_usage[]      = { "sort pileup with counters", NULL };
static const char * func_stat_usage[]       = { "strand/tlen statistic", NULL };
static const char * func_mismatch_usage[]   = { "only lines with mismatch", NULL };
static const char * func_index_usage[]      = { "list deletion counts", NULL };
static const char * func_indels_usage[]     = { "list only inserts/deletions", NULL };

static const char * func_varcount_usage[]   = { "variation counters: ",
                                                "ref-name, ref-pos, ref-base, coverage, ",
                                                "mismatch A, mismatch C, mismatch G, mismatch T,",
                                                "deletes, inserts,",
                                                "ins after A, ins after C, ins after G, ins after T", NULL };

static const char * func_deletes_usage[]    = { "list deletions greater then 20", NULL };

static const char * func_usage[]            = { "alternative functionality", NULL };

static const char * ngc_usage[] = { "path to ngc file", NULL };

OptDef MyOptions[] =
{
    /*name,           	alias,         	hfkt,	usage-help,		maxcount, needs value, required */
    { OPTION_MINMAPQ,	ALIAS_MINMAPQ,	NULL,	minmapq_usage,	1,        true,        false },
    { OPTION_DUPS,		ALIAS_DUPS,		NULL,	dups_usage,		1,        true,        false },
    { OPTION_NOSKIP,	ALIAS_NOSKIP,	NULL,	noskip_usage,	1,        false,       false },
    { OPTION_SHOWID,	ALIAS_SHOWID,	NULL,	showid_usage,	1,        false,       false },
    { OPTION_SPOTGRP,	ALIAS_SPOTGRP,	NULL,	spotgrp_usage,	1,        false,       false },
	{ OPTION_DEPTH_PER_SPOTGRP, NULL,	NULL,	dpgrp_usage,	1,        false,       false },
    { OPTION_SEQNAME,	ALIAS_SEQNAME,	NULL,	seqname_usage,	1,        false,       false },
    { OPTION_MIN_M,		NULL,			NULL,	min_m_usage,	1,        true,        false },
    { OPTION_MERGE,		NULL,			NULL,	merge_usage,	1,        true,        false },
    { OPTION_FUNC,		ALIAS_FUNC,		NULL,	func_usage,		1,        true,        false },
    { OPTION_NGC,       NULL,           NULL,   ngc_usage, 1, true, false },
};

/* =========================================================================================== */

typedef struct pileup_callback_data {
    const AlignMgr *almgr;
    pileup_options *options;    /* pileup_options.h */
} pileup_callback_data;


/* =========================================================================================== */

static rc_t get_str_option( const Args *args, const char *name, const char ** res ) {
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    *res = NULL;
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "ArgsOptionCount() failed" );
    } else {
        if ( count > 0 ) {
            rc = ArgsOptionValue( args, name, 0, (const void **)res );
            if ( rc != 0 ) {
                LOGERR( klogInt, rc, "ArgsOptionValue() failed" );
            }
        }
    }
    return rc;
}

static rc_t get_uint32_option( const Args *args, const char *name,
                        uint32_t *res, const uint32_t def ) {
    const char * s;
    rc_t rc = get_str_option( args, name, &s );
    if ( rc == 0 && s != NULL ) {
        *res = atoi( s );
    } else {
        *res = def;
    }
    return rc;
}

static rc_t get_bool_option( const Args *args, const char *name, bool *res, const bool def ) {
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc == 0 && count > 0 ) {
        *res = true;
    } else {
        *res = def;
    }
    return rc;
}

static int cmp_pchar( const char * a, const char * b ) {
    int res = -1;
    if ( ( a != NULL )&&( b != NULL ) )  {
        size_t len_a = string_size( a );
        size_t len_b = string_size( b );
        res = string_cmp ( a, len_a, b, len_b, ( len_a < len_b ) ? len_b : len_a );
    }
    return res;
}

/* =========================================================================================== */

static rc_t get_pileup_options( Args * args, pileup_options *opts )
{
    rc_t rc = get_common_options( args, &opts->cmn ); /* cmdline_cmn.h */
    opts -> function = sra_pileup_samtools; /* above */

    if ( rc == 0 ) {
        rc = get_uint32_option( args, OPTION_MINMAPQ, &opts->minmapq, 0 );
    }
    if ( rc == 0 ) {
        rc = get_uint32_option( args, OPTION_MIN_M, &opts->min_mismatch, 5 );
    }
    if ( rc == 0 ) {
        rc = get_uint32_option( args, OPTION_MERGE, &opts->merge_dist, 10000 );
    }
    if ( rc == 0 ) {
        rc = get_bool_option( args, OPTION_DUPS, &opts->process_dups, false );
    }
    if ( rc == 0 ) {
        rc = get_bool_option( args, OPTION_NOSKIP, &opts->no_skip, false );
    }
    if ( rc == 0 ) {
        rc = get_bool_option( args, OPTION_SHOWID, &opts->show_id, false );
    }
    if ( rc == 0 ) {
        rc = get_bool_option( args, OPTION_SPOTGRP, &opts->div_by_spotgrp, false );
    }
    if ( rc == 0 ) {
        rc = get_bool_option( args, OPTION_DEPTH_PER_SPOTGRP, &opts->depth_per_spotgrp, false );
    }
    if ( rc == 0 ) {
        rc = get_bool_option( args, OPTION_SEQNAME, &opts->use_seq_name, false );
    }
    if ( rc == 0 ) {
        const char * fkt = NULL;
        rc = get_str_option( args, OPTION_FUNC, &fkt );
        if ( rc == 0 && fkt != NULL ) {
            if ( cmp_pchar( fkt, FUNC_COUNTERS ) == 0 ) {
                opts -> function = sra_pileup_counters;
            } else if ( cmp_pchar( fkt, FUNC_STAT ) == 0 ) {
                opts -> function = sra_pileup_stat;
            } else if ( cmp_pchar( fkt, FUNC_RE_REF ) == 0 ) {
                opts -> function = sra_pileup_report_ref;
            } else if ( cmp_pchar( fkt, FUNC_RE_REF_EXT ) == 0 ) {
                opts -> function = sra_pileup_report_ref_ext;
            } else if ( cmp_pchar( fkt, FUNC_DEBUG ) == 0 ) {
                opts -> function = sra_pileup_debug;
            } else if ( cmp_pchar( fkt, FUNC_MISMATCH ) == 0 ) {
                opts -> function = sra_pileup_mismatch;
            } else if ( cmp_pchar( fkt, FUNC_INDEX ) == 0 ) {
                opts -> function = sra_pileup_index;
            } else if ( cmp_pchar( fkt, FUNC_TEST ) == 0 ) {
                opts -> function = sra_pileup_test;
            } else if ( cmp_pchar( fkt, FUNC_VARCOUNT ) == 0 ) {
                opts -> function = sra_pileup_varcount;
            } else if ( cmp_pchar( fkt, FUNC_DELETES ) == 0 ) {
                opts -> function = sra_pileup_deletes;
            } else if ( cmp_pchar( fkt, FUNC_INDELS ) == 0 ) {
                opts -> function = sra_pileup_indels;
            }
        }
    }

    if ( rc == 0 ) {
        const char * ngc = NULL;
        rc = get_str_option( args, OPTION_NGC, &ngc );
        if ( rc == 0 && ngc != NULL ) {
            KConfigSetNgcFile(ngc);
        }
    }
    return rc;
}

/* GLOBAL VARIABLES */
struct {
    KWrtWriter org_writer;
    void* org_data;
    KFile* kfile;
    uint64_t pos;
} g_out_writer = { NULL };

const char UsageDefaultName[] = "sra-pileup";

rc_t CC UsageSummary ( const char * progname ) {
    return KOutMsg( "\n"
                    "Usage:\n"
                    "  %s <path> [options]\n"
                    "\n", progname );
}

rc_t CC Usage ( const Args * args ) {
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL ) {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    } else {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }
    if ( rc != 0 ) {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary ( progname );
    KOutMsg ( "Options:\n" );
    print_common_helplines();
    HelpOptionLine ( NULL, OPTION_NGC, "path", ngc_usage );
    HelpOptionLine ( ALIAS_MINMAPQ, OPTION_MINMAPQ, "min. mapq", minmapq_usage );
    HelpOptionLine ( ALIAS_DUPS, OPTION_DUPS, "dup-mode", dups_usage );
    HelpOptionLine ( ALIAS_SPOTGRP, OPTION_SPOTGRP, NULL, spotgrp_usage );
    HelpOptionLine ( NULL, OPTION_DEPTH_PER_SPOTGRP, NULL, dpgrp_usage );
    HelpOptionLine ( ALIAS_SEQNAME, OPTION_SEQNAME, NULL, seqname_usage );
    HelpOptionLine ( NULL, OPTION_MIN_M, NULL, min_m_usage );
    HelpOptionLine ( NULL, OPTION_MERGE, NULL, merge_usage );

    HelpOptionLine ( NULL, "function ref",      NULL, func_ref_usage );
    HelpOptionLine ( NULL, "function ref-ex",   NULL, func_ref_ex_usage );
    HelpOptionLine ( NULL, "function count",    NULL, func_count_usage );
    HelpOptionLine ( NULL, "function stat",     NULL, func_stat_usage );
    HelpOptionLine ( NULL, "function mismatch", NULL, func_mismatch_usage );
    HelpOptionLine ( NULL, "function index",    NULL, func_index_usage );
    HelpOptionLine ( NULL, "function varcount", NULL, func_varcount_usage );
    HelpOptionLine ( NULL, "function deletes",  NULL, func_deletes_usage );
    HelpOptionLine ( NULL, "function indels",   NULL, func_indels_usage );

    KOutMsg ( "\nGrouping of accessions into artificial spotgroups:\n" );
    KOutMsg ( "  sra-pileup SRRXXXXXX=a SRRYYYYYY=b SRRZZZZZZ=a\n\n" );

    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );
    return rc;
}


/* =========================================================================================== */

/*
static rc_t CC BufferedWriter ( void* self, const char* buffer, size_t bufsize, size_t* num_writ ) {
    rc_t rc = 0;

    assert( buffer != NULL );
    assert( num_writ != NULL );

    do {
        rc = KFileWrite( g_out_writer.kfile, g_out_writer.pos, buffer, bufsize, num_writ );
        if ( rc == 0 ) {
            buffer += *num_writ;
            bufsize -= *num_writ;
            g_out_writer.pos += *num_writ;
        }
    } while ( rc == 0 && bufsize > 0 );
    return rc;
}

static rc_t set_stdout_to( bool gzip, bool bzip2, const char * filename, size_t bufsize ) {
    rc_t rc = 0;
    if ( gzip && bzip2 ) {
        rc = RC( rcApp, rcFile, rcConstructing, rcParam, rcAmbiguous );
    } else {
        KDirectory *dir;
        rc = KDirectoryNativeDir( &dir );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "KDirectoryNativeDir() failed" );
        } else {
            KFile *of;
            rc = KDirectoryCreateFile ( dir, &of, false, 0664, kcmInit, "%s", filename );
            if ( rc == 0 ) {
                KFile *buf;
                if ( gzip ) {
                    KFile *gz;
                    rc = KFileMakeGzipForWrite( &gz, of );
                    if ( rc == 0 ) {
                        KFileRelease( of );
                        of = gz;
                    }
                }
                if ( bzip2 ) {
                    KFile *bz;
                    rc = KFileMakeBzip2ForWrite( &bz, of );
                    if ( rc == 0 ) {
                        KFileRelease( of );
                        of = bz;
                    }
                }
                rc = KBufFileMakeWrite( &buf, of, false, bufsize );
                if ( rc == 0 ) {
                    g_out_writer.kfile = buf;
                    g_out_writer.org_writer = KOutWriterGet();
                    g_out_writer.org_data = KOutDataGet();
                    rc = KOutHandlerSet( BufferedWriter, &g_out_writer );
                    if ( rc != 0 ) {
                        LOGERR( klogInt, rc, "KOutHandlerSet() failed" );
                    }
                }
                KFileRelease( of );
            }
            KDirectoryRelease( dir );
        }
    }
    return rc;
}

static void release_stdout_redirection( void ) {
    KFileRelease( g_out_writer.kfile );
    if( g_out_writer.org_writer != NULL ) {
        KOutHandlerSet( g_out_writer.org_writer, g_out_writer.org_data );
    }
    g_out_writer.org_writer = NULL;
}
*/

static rc_t CC write_to_FILE( void *f, const char *buffer, size_t bytes, size_t *num_writ ) {
    * num_writ = fwrite ( buffer, 1, bytes, f );
    if ( * num_writ != bytes ) {
        return RC( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    }
    return 0;
}

/* =========================================================================================== */

static rc_t read_base_and_len( struct VCursor const *curs,
                               uint32_t column_idx,
                               int64_t row_id,
                               const void ** base,
                               uint32_t * len ) {
    uint32_t elem_bits, boff, len_intern;
    const void * ptr;
    rc_t rc = VCursorCellDataDirect ( curs, row_id, column_idx, &elem_bits, &ptr, &boff, &len_intern );
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "VCursorCellDataDirect() failed" );
    } else {
        if ( len != NULL ) *len = len_intern;
        if ( base != NULL ) *base = ptr;
    }
    return rc;
}

static rc_t CC populate_tooldata( void *obj, const PlacementRecord *placement,
        struct VCursor const *curs, INSDC_coord_zero ref_window_start, INSDC_coord_len ref_window_len,
        void *data, void * placement_ctx ) {
    tool_rec * rec = ( tool_rec * ) obj;
    pileup_callback_data * cb_data = ( pileup_callback_data * )data;
    pileup_col_ids * col_ids = placement_ctx;
    rc_t rc = 0;

    rec -> quality = NULL;
    if ( !( cb_data -> options -> process_dups ) )
    {
        const uint8_t * read_filter;
        uint32_t read_filter_len;
        rc = read_base_and_len( curs, col_ids -> idx_read_filter, placement -> id,
                                ( const void ** )&read_filter, &read_filter_len );
        if ( rc == 0 ) {
            if ( ( *read_filter == SRA_READ_FILTER_REJECT )||
                 ( *read_filter == SRA_READ_FILTER_CRITERIA ) ) {
                rc = RC( rcAlign, rcType, rcAccessing, rcId, rcIgnored );
            }
        }
    }

    if ( rc == 0 ) {
        const bool * orientation;
        rc = read_base_and_len( curs, col_ids -> idx_ref_orientation, placement -> id,
                                ( const void ** )&orientation, NULL );
        if ( rc == 0 ) {
            rec -> reverse = *orientation;
        }
    }

    if ( rc == 0 && !( cb_data -> options -> cmn . omit_qualities ) )
    {
        const uint8_t * quality;

        rc = read_base_and_len( curs, col_ids -> idx_quality, placement -> id,
                                ( const void ** )&quality, &( rec -> quality_len ) );
        if ( rc == 0 ) {
            rec -> quality = ( uint8_t * )rec;
            rec -> quality += sizeof ( * rec );
            /* we have to copy, because the cursor moves on - invalidating the pointer! */
            memmove( rec -> quality, quality, rec -> quality_len );
        }
    }

    if ( rc == 0 && cb_data -> options -> read_tlen ) {
        const int32_t * tlen;
        uint32_t tlen_len;

        rc = read_base_and_len( curs, col_ids -> idx_template_len, placement -> id,
                                (const void **)&tlen, &tlen_len );
        if ( rc == 0 && tlen_len > 0 ) {
            rec -> tlen = *tlen;
        } else {
            rec -> tlen = 0;
        }
    } else {
        rec -> tlen = 0;
    }

    return rc;
}


static rc_t CC alloc_size( struct VCursor const *curs, int64_t row_id, size_t * size, void *data, void * placement_ctx ) {
    rc_t rc = 0;
    tool_rec * rec;
    pileup_callback_data *cb_data = ( pileup_callback_data * )data;
    pileup_col_ids * col_ids = placement_ctx;
    *size = ( sizeof *rec );

    if ( !( cb_data -> options -> cmn . omit_qualities ) ) {
        uint32_t q_len;
        rc = read_base_and_len( curs, col_ids -> idx_quality, row_id, NULL, &q_len );
        if ( rc == 0 ) { *size += q_len; }
    }
    return rc;
}

static rc_t walk_ref_position( ReferenceIterator *ref_iter,
                               const PlacementRecord *rec,
                               struct dyn_string *line,
                               char * qual,
                               pileup_options *options ) {
    rc_t rc = 0;
    INSDC_coord_zero seq_pos;
    int32_t state = ReferenceIteratorState ( ref_iter, &seq_pos );
    tool_rec *xrec = ( tool_rec * ) PlacementRecordCast ( rec, placementRecordExtension1 );
    bool reverse = xrec->reverse;

    if ( !( options -> cmn . omit_qualities ) ) {
        if ( seq_pos < xrec->quality_len ) {
            *qual = xrec->quality[ seq_pos ];
        } else {
            *qual = 2;
        }
    }

    if ( ( state & align_iter_invalid ) == align_iter_invalid ) {
        return ds_add_char( line, '?' );
    }

    if ( ( state & align_iter_first ) == align_iter_first ) {
        char s[ 3 ];
        int32_t c = rec -> mapq + 33;
        if ( c > '~' ) { c = '~'; }
        if ( c < 33 ) { c = 33; }
        s[ 0 ] = '^';
        s[ 1 ] = c;
        s[ 2 ] = 0;
        rc = ds_add_str( line, s );
    }

    if ( rc == 0 ) {
        if ( ( state & align_iter_skip ) == align_iter_skip ) {
            if ( reverse ) {
                rc = ds_add_char( line, '<' );
            } else {
                rc = ds_add_char( line, '>' );
            }
            if ( !( options -> cmn . omit_qualities ) )
                *qual = xrec -> quality[ seq_pos + 1 ];
        } else {
            if ( ( state & align_iter_match ) == align_iter_match ) {
                rc = ds_add_char( line, ( reverse ? ',' : '.' ) );
            } else {
                rc = ds_add_char( line, _4na_to_ascii( state, reverse ) );
            }
        }
    }

    if ( ( state & align_iter_insert ) == align_iter_insert ) {
        const INSDC_4na_bin *bases;
        uint32_t i;
        uint32_t n = ReferenceIteratorBasesInserted ( ref_iter, &bases );

        rc = ds_add_fmt( line, "+%u", n );
        for ( i = 0; i < n && rc == 0; ++i ) {
            rc = ds_add_char( line, _4na_to_ascii( bases[ i ], reverse ) );
        }
    }

    if ( ( state & align_iter_delete ) == align_iter_delete ) {
        const INSDC_4na_bin *bases;
        INSDC_coord_zero ref_pos;
        uint32_t n = ReferenceIteratorBasesDeleted ( ref_iter, &ref_pos, &bases );
        if ( bases != NULL ) {
            uint32_t i;
            rc = ds_add_fmt( line, "-%u", n );
            for ( i = 0; i < n && rc == 0; ++i )  {
                rc = ds_add_char( line, _4na_to_ascii( bases[ i ], reverse ) );
            }
            free( (void *) bases );
        }
    }

    if ( ( ( state & align_iter_last ) == align_iter_last )&& ( rc == 0 ) ) {
        rc = ds_add_char( line, '$' );
    }

    if ( options -> show_id ) {
        rc = ds_add_fmt( line, "(%,lu:%,d-%,d/%u)",
                         rec -> id, rec -> pos + 1, rec -> pos + rec -> len, seq_pos );
    }

    return rc;
}

static rc_t walk_alignments( ReferenceIterator *ref_iter,
                             struct dyn_string *line,
                             struct dyn_string *events,
                             struct dyn_string *qualities,
                             pileup_options *options ) {
    uint32_t depth = 0;
    rc_t rc = 0;
    bool done = false;

    ds_reset( events );
    do {
        const PlacementRecord *rec;
        /* double purpose of rc2 !!! signals done as well as errors... */
        rc_t rc2 = ReferenceIteratorNextPlacement ( ref_iter, &rec );
        if ( 0 == rc2 ) {
            rc = walk_ref_position( ref_iter, rec, events, ds_get_char( qualities, depth++ ), options );
        } else if ( GetRCState( rc2 ) == rcDone ) {
            done = true;
        } else {
            rc = rc2;
        }
    } while ( rc == 0 && !done );

    if ( 0 == rc ) {
        rc = Quitting();
    }

    if ( 0 == rc && options -> depth_per_spotgrp ) {
        rc = ds_add_fmt( line, "%d\t", depth );
    }

    if ( 0 == rc ) {
        rc = ds_add_ds( line, events );
    }

    if ( 0 == rc && !( options -> cmn . omit_qualities ) ) {
        uint32_t i;
        rc = ds_add_char( line, '\t' );
        for ( i = 0; 0 == rc && i < depth; ++i ) {
            char * c = ds_get_char( qualities, i );
            rc = ds_add_char( line, *c + 33 );
        }
    }

    return rc;
}

static rc_t walk_spot_groups( ReferenceIterator *ref_iter,
                              struct dyn_string *line,
                              struct dyn_string *events,
                              struct dyn_string *qualities,
                              pileup_options *options ) {
    rc_t rc = 0;
    bool done = false;

    ds_reset( events );
    do
    {
        /* double purpose of rc2 !!! signals done as well as errors... */
        rc_t rc2 = ReferenceIteratorNextSpotGroup ( ref_iter, NULL, NULL );
        if ( 0 == rc2 ) {
            rc = ds_add_char( line, '\t' );
            if ( rc == 0 ) {
                rc = walk_alignments( ref_iter, line, events, qualities, options );
            }
        } else if ( GetRCState( rc2 ) == rcDone ) {
            done = true;
        } else {
            rc = rc2;
        }
    } while ( rc == 0 && !done );

    return rc;
}

static rc_t walk_position( ReferenceIterator *ref_iter,
                           const char * refname,
                           struct dyn_string *line,
                           struct dyn_string *events,
                           struct dyn_string *qualities,
                           pileup_options *options ) {
    INSDC_coord_zero pos;
    uint32_t depth;
    INSDC_4na_bin base;

    /* double purpose of rc !!! signals done as well as errors... */
    rc_t rc = ReferenceIteratorPosition ( ref_iter, &pos, &depth, &base );
    if ( rc != 0 ) {
        if ( GetRCState( rc ) != rcDone ) {
            LOGERR( klogInt, rc, "ReferenceIteratorNextPos() failed" );
        }
    } else if ( ( depth > 0 )||( options -> no_skip ) ) {
        bool skip = skiplist_is_skip_position( options -> skiplist, pos + 1 );
        if ( !skip ) {
            rc = ds_expand( line, ( 5 * depth ) + 100 );
            if ( rc == 0 ) {
                rc = ds_expand( events, ( 5 * depth ) + 100 );
                if ( rc == 0 ) {
                    rc = ds_expand( qualities, depth + 100 );
                    if ( rc == 0 ) {
                        char c = _4na_to_ascii( base, false );

                        ds_reset( line );

                        if ( options -> depth_per_spotgrp ) {
                            rc = ds_add_fmt( line, "%s\t%u\t%c", refname, pos + 1, c );
                        } else {
                            rc = ds_add_fmt( line, "%s\t%u\t%c\t%u", refname, pos + 1, c, depth );
                        }
                        if ( rc == 0 ) {
                            if ( depth > 0 ) {
                                rc = walk_spot_groups( ref_iter, line, events, qualities, options );
                            }
                            /* only one KOutMsg() per line... */
                            if ( rc == 0 ) {
                                rc = KOutMsg( "%s\n", ds_get_char( line, 0 ) );
                            }
                            if ( GetRCState( rc ) == rcDone ) { rc = 0; }
                        }
                    }
                }
            }
        }
    }
    return rc;
}

static rc_t walk_reference_window( ReferenceIterator *ref_iter,
                                   const char * refname,
                                   struct dyn_string *line,
                                   struct dyn_string *events,
                                   struct dyn_string *qualities,
                                   pileup_options *options ) {
    rc_t rc = 0;
    while ( rc == 0 ) {
        rc = ReferenceIteratorNextPos ( ref_iter, !options->no_skip );
        if ( rc != 0 ) {
            if ( GetRCState( rc ) != rcDone ) {
                LOGERR( klogInt, rc, "ReferenceIteratorNextPos() failed" );
            }
        } else {
            rc = walk_position( ref_iter, refname, line, events, qualities, options );
        }
        if ( rc == 0 ) {
            rc = Quitting();
        }
    }
    if ( GetRCState( rc ) == rcDone ) { rc = 0; }
    return rc;
}

static rc_t walk_reference( ReferenceIterator *ref_iter,
                            const char * refname,
                            pileup_options *options ) {
    struct dyn_string * line;
    rc_t rc = ds_allocate( &line, 4096 );
    if ( rc == 0 ) {
        struct dyn_string * events;
        /*rc_t*/ rc = ds_allocate( &events, 4096 );
        if ( rc == 0 ) {
            struct dyn_string * qualities;
            rc = ds_allocate( &qualities, 4096 );
            if ( rc == 0 ) {
                while ( rc == 0 ) {
                    rc = Quitting ();
                    if ( rc == 0 ) {
                        INSDC_coord_zero first_pos;
                        INSDC_coord_len len;
                        rc = ReferenceIteratorNextWindow ( ref_iter, &first_pos, &len );
                        if ( rc != 0 ) {
                            if ( GetRCState( rc ) != rcDone ) {
                                LOGERR( klogInt, rc, "ReferenceIteratorNextWindow() failed" );
                            }
                        } else {
                            rc = walk_reference_window( ref_iter, refname, line, events, qualities, options );
                        }
                    }
                }
                ds_free( qualities );
            }
            ds_free( events );
        }
        ds_free( line );
    }
    if ( GetRCState( rc ) == rcDone ) { rc = 0; }
    return rc;
}

/* =========================================================================================== */

static rc_t walk_ref_iter( ReferenceIterator *ref_iter, pileup_options *options ) {
    rc_t rc = 0;
    while( rc == 0 ) {
        /* this is the 1st level of walking the reference-iterator:
           visiting each (requested) reference */
        struct ReferenceObj const * refobj;
        rc = ReferenceIteratorNextReference( ref_iter, NULL, NULL, &refobj );
        if ( rc == 0 ) {
            if ( refobj != NULL ) {
                /* we need both: seq-name ( for inst: chr1 ) and seq-id ( NC.... )
                   to perform a correct lookup into the skiplist */
                const char * seq_name = NULL;
                rc = ReferenceObj_Name( refobj, &seq_name );
                if ( rc != 0 ) {
                    LOGERR( klogInt, rc, "ReferenceObj_Name() failed" );
                } else {
                    const char * seq_id = NULL;
                    rc = ReferenceObj_SeqId( refobj, &seq_id );
                    if ( rc != 0 ) {
                        LOGERR( klogInt, rc, "ReferenceObj_SeqId() failed" );
                    } else {
                        const char * refname = options->use_seq_name ? seq_name : seq_id;

                        if ( options -> skiplist != NULL ) {
                            skiplist_enter_ref( options -> skiplist, seq_name, seq_id ); /* ref_regions.c */
                        }
                        rc = walk_reference( ref_iter, refname, options );
                    }
                }
            }
        } else {
            if ( GetRCState( rc ) != rcDone ) {
                LOGERR( klogInt, rc, "ReferenceIteratorNextReference() failed" );
            }
        }
    }
    if ( GetRCState( rc ) == rcDone ) { rc = 0; }
    if ( GetRCState( rc ) == rcCanceled ) { rc = 0; }
    return rc;
}

/* =========================================================================================== */

static rc_t add_quality_and_orientation( const VTable *tbl, const VCursor ** cursor,
                                         bool omit_qualities, bool read_tlen, pileup_col_ids * cursor_ids ) {
    rc_t rc = VTableCreateCursorRead ( tbl, cursor );
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "VTableCreateCursorRead() failed" );
    }

    if ( rc == 0 && !omit_qualities ) {
        rc = VCursorAddColumn ( *cursor, &cursor_ids->idx_quality, COL_QUALITY );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "VCursorAddColumn(QUALITY) failed" );
        }
    }

    if ( rc == 0 ) {
        rc = VCursorAddColumn ( *cursor, &cursor_ids->idx_ref_orientation, COL_REF_ORIENTATION );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "VCursorAddColumn(REF_ORIENTATION) failed" );
        }
    }

    if ( rc == 0 ) {
        rc = VCursorAddColumn ( *cursor, &cursor_ids->idx_read_filter, COL_READ_FILTER );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "VCursorAddColumn(READ_FILTER) failed" );
        }
    }

    if ( rc == 0 && read_tlen ) {
        rc = VCursorAddColumn ( *cursor, &cursor_ids->idx_template_len, COL_TEMPLATE_LEN );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "VCursorAddColumn(TEMPLATE_LEN) failed" );
        }
    }
    return rc;
}

static rc_t prepare_prim_cursor( const VDatabase *db, const VCursor ** cursor,
                                 bool omit_qualities, bool read_tlen, pileup_col_ids * cursor_ids ) {
    const VTable *tbl;
    rc_t rc = VDatabaseOpenTableRead ( db, &tbl, "PRIMARY_ALIGNMENT" );
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "VDatabaseOpenTableRead(PRIMARY_ALIGNMENT) failed" );
    } else {
        rc = add_quality_and_orientation( tbl, cursor, omit_qualities, read_tlen, cursor_ids );
        VTableRelease ( tbl );
    }
    return rc;
}

static rc_t prepare_sec_cursor( const VDatabase *db, const VCursor ** cursor,
                                bool omit_qualities, bool read_tlen, pileup_col_ids * cursor_ids ) {
    const VTable *tbl;
    rc_t rc = VDatabaseOpenTableRead ( db, &tbl, "SECONDARY_ALIGNMENT" );
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "VDatabaseOpenTableRead(SECONDARY_ALIGNMENT) failed" );
    } else {
        rc = add_quality_and_orientation( tbl, cursor, omit_qualities, read_tlen, cursor_ids );
        VTableRelease ( tbl );
    }
    return rc;
}

static rc_t prepare_evidence_cursor( const VDatabase *db, const VCursor ** cursor,
                                     bool omit_qualities, bool read_tlen, pileup_col_ids * cursor_ids ) {
    const VTable *tbl;
    rc_t rc = VDatabaseOpenTableRead ( db, &tbl, "EVIDENCE_ALIGNMENT" );
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "VDatabaseOpenTableRead(EVIDENCE) failed" );
    } else {
        rc = add_quality_and_orientation( tbl, cursor, omit_qualities, read_tlen, cursor_ids );
        VTableRelease ( tbl );
    }
    return rc;
}

#if 0
static void show_placement_params( const char * prefix, const ReferenceObj *refobj,
                                   uint32_t start, uint32_t end ) {
    const char * name;
    rc_t rc = ReferenceObj_SeqId( refobj, &name );
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "ReferenceObj_SeqId() failed" );
    } else {
        KOutMsg( "prepare %s: <%s> %u..%u\n", prefix, name, start, end ) ;
    }
}
#endif

static rc_t make_cursor_ids( Vector *cursor_id_vector, pileup_col_ids ** cursor_ids ) {
    rc_t rc;
    pileup_col_ids * ids = calloc( 1, sizeof * ids );
    if ( ids == NULL ) {
        rc = RC ( rcApp, rcNoTarg, rcOpening, rcMemory, rcExhausted );
    } else {
        rc = VectorAppend ( cursor_id_vector, NULL, ids );
        if ( rc != 0 ) {
            free( ids );
        } else {
            *cursor_ids = ids;
        }
    }
    return rc;
}

static bool row_not_found_while_reading_column( rc_t rc ) {
    return ( rcVDB == GetRCModule( rc ) &&
             rcColumn == GetRCTarget( rc ) &&
             rcReading == GetRCContext( rc ) &&
             rcRow == GetRCObject( rc ) &&
             rcNotFound == GetRCState( rc ) );
}

static rc_t CC prepare_section_cb( prepare_ctx * ctx, const struct reference_range * range ) {
    rc_t rc = 0;
    INSDC_coord_len len;
    if ( ctx -> db == NULL || ctx -> refobj == NULL ) {
        rc = SILENT_RC ( rcApp, rcNoTarg, rcOpening, rcSelf, rcInvalid );
        /* it is opened in prepare_db_table even if ctx->db == NULL */
        PLOGERR( klogErr, ( klogErr, rc, "failed to process $(path)",
            "path=%s", ctx->path == NULL ? "input argument" : ctx->path));
        ReportSilence();
    } else {
        rc = ReferenceObj_SeqLength( ctx->refobj, &len );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "ReferenceObj_SeqLength() failed" );
        } else {
            uint32_t start, end;
            rc_t rc1 = 0, rc2 = 0, rc3 = 0;

            if ( range == NULL ) {
                start = 1;
                end = ( len - start ) + 1;
            } else {
                start = get_ref_range_start( range );
                end   = get_ref_range_end( range );
            }

            if ( start == 0 ) { start = 1; }
            if ( ( end == 0 )||( end > len + 1 ) ) { end = ( len - start ) + 1; }

            /* depending on ctx->select prepare primary, secondary or both... */
            if ( ctx->use_primary_alignments ) {
                if ( ctx->prim_cur == NULL )
                {
                    rc1 = make_cursor_ids( ctx->data, &ctx->prim_cur_ids );
                    if ( rc1 != 0 ) {
                        LOGERR( klogInt, rc1, "cannot create cursor-ids for prim. alignment cursor" );
                    } else {
                        rc1 = prepare_prim_cursor( ctx -> db, &( ctx -> prim_cur ), ctx -> omit_qualities,
                                                   ctx -> read_tlen, ctx -> prim_cur_ids );
                    }
                }

                if ( rc1 == 0 ) {
                    /* show_placement_params( "primary", ctx->refobj, start, end ); */
                    rc1 = ReferenceIteratorAddPlacements( ctx -> ref_iter,       /* the outer ref-iter */
                                                          ctx -> refobj,         /* the ref-obj for this chromosome */
                                                          start - 1,             /* start ( zero-based ) */
                                                          end - start + 1,       /* length */
                                                          NULL,                  /* ref-cursor */
                                                          ctx -> prim_cur,       /* align-cursor */
                                                          primary_align_ids,     /* which id's */
                                                          ctx -> spot_group,     /* what read-group */
                                                          ctx -> prim_cur_ids    /* placement-context */
                                                         );
                    if ( rc1 != 0 && !row_not_found_while_reading_column( rc1 ) ) {
                        /* row_not_found_while_reading column within VDB happens if the
                         requested reference-slice is empty, let's silence that */
                        LOGERR( klogInt, rc1, "ReferenceIteratorAddPlacements(prim) failed" );
                    }
                }
            }

            if ( ctx -> use_secondary_alignments ) {
                if ( ctx -> sec_cur == NULL ) {
                    rc2 = make_cursor_ids( ctx -> data, &( ctx -> sec_cur_ids ) );
                    if ( rc2 != 0 ) {
                        LOGERR( klogInt, rc2, "cannot create cursor-ids for sec. alignment cursor" );
                    } else {
                        rc2 = prepare_sec_cursor( ctx -> db, &( ctx -> sec_cur ), ctx -> omit_qualities,
                                                  ctx -> read_tlen, ctx -> sec_cur_ids );
                    }
                }

                if ( rc2 == 0 ) {
                    /* show_placement_params( "secondary", ctx->refobj, start, end ); */
                    rc2 = ReferenceIteratorAddPlacements( ctx -> ref_iter,       /* the outer ref-iter */
                                                          ctx -> refobj,         /* the ref-obj for this chromosome */
                                                          start - 1,             /* start ( zero-based ) */
                                                          end - start + 1,       /* length */
                                                          NULL,                  /* ref-cursor */
                                                          ctx -> sec_cur,        /* align-cursor */
                                                          secondary_align_ids,   /* which id's */
                                                          ctx -> spot_group,     /* what read-group */
                                                          ctx -> sec_cur_ids     /* placement-context */
                                                         );
                    if ( rc2 != 0 && !row_not_found_while_reading_column( rc2 ) ) {
                        /* row_not_found_while_reading column within VDB happens if the
                         requested reference-slice is empty, let's silence that */
                        LOGERR( klogInt, rc2, "ReferenceIteratorAddPlacements(sec) failed" );
                    }
                }
            }

            if ( ctx -> use_evidence_alignments ) {
                if ( ctx -> ev_cur == NULL ) {
                    rc3 = make_cursor_ids( ctx -> data, &( ctx -> ev_cur_ids ) );
                    if ( rc3 != 0 ) {
                        LOGERR( klogInt, rc3, "cannot create cursor-ids for ev. alignment cursor" );
                    } else {
                        rc3 = prepare_evidence_cursor( ctx -> db, &( ctx -> ev_cur ), ctx -> omit_qualities,
                                                       ctx -> read_tlen, ctx -> ev_cur_ids );
                    }
                }

                if ( rc3 == 0 ) {
                    /* show_placement_params( "evidende", ctx->refobj, start, end ); */
                    rc3 = ReferenceIteratorAddPlacements( ctx -> ref_iter,       /* the outer ref-iter */
                                                          ctx -> refobj,         /* the ref-obj for this chromosome */
                                                          start - 1,             /* start ( zero-based ) */
                                                          end - start + 1,       /* length */
                                                          NULL,                  /* ref-cursor */
                                                          ctx -> ev_cur,         /* align-cursor */
                                                          evidence_align_ids,    /* which id's */
                                                          ctx -> spot_group,     /* what read-group */
                                                          ctx -> ev_cur_ids      /* placement-context */
                                                         );
                    if ( rc3 != 0 && !row_not_found_while_reading_column( rc3 ) ) {
                        /* row_not_found_while_reading column within VDB happens if the
                         requested reference-slice is empty, let's silence that */
                        LOGERR( klogInt, rc3, "ReferenceIteratorAddPlacements(evidence) failed" );
                    }
                }
            }

            if ( rc1 == SILENT_RC( rcAlign, rcType, rcAccessing, rcRow, rcNotFound ) ) {
                rc = rc1; /* from allocate_populate_rec */
            } else if ( rc1 == 0 ) {
                rc = 0;
            } else if ( rc2 == 0 ) {
                rc = 0;
            } else if ( rc3 == 0 ) {
                rc = 0;
            } else {
                rc = rc1;
            }
        }
    }
    return rc;
}

typedef struct foreach_arg_ctx {
    pileup_options *options;
    const VDBManager *vdb_mgr;
    VSchema *vdb_schema;
    ReferenceIterator *ref_iter;
    BSTree *ranges;
    Vector *cursor_ids;
} foreach_arg_ctx;


/* called for each source-file/accession */
static rc_t CC on_argument( const char * path, const char * spot_group, void * data ) {
    rc_t rc = 0;
    foreach_arg_ctx * ctx = ( foreach_arg_ctx * )data;

    int path_type = ( VDBManagerPathType ( ctx -> vdb_mgr, "%s", path ) & ~ kptAlias );
    ReportResetObject ( path );
    if ( path_type != kptDatabase ) {
        rc = RC ( rcApp, rcNoTarg, rcOpening, rcItem, rcUnsupported );
        PLOGERR( klogErr, ( klogErr, rc, "failed to open '$(path)', it is not a vdb-database", "path=%s", path ) );
    } else {
        const VDatabase *db;
        rc = VDBManagerOpenDBRead ( ctx -> vdb_mgr, &db, ctx -> vdb_schema, "%s", path );
        if ( rc != 0 ) {
            rc = RC ( rcApp, rcNoTarg, rcOpening, rcItem, rcUnsupported );
            PLOGERR( klogErr, ( klogErr, rc, "failed to open '$(path)'", "path=%s", path ) );
        } else {
            bool is_csra = VDatabaseIsCSRA ( db );
            VDatabaseRelease ( db );
            if ( !is_csra ) {
                rc = RC ( rcApp, rcNoTarg, rcOpening, rcItem, rcUnsupported );
                PLOGERR( klogErr, ( klogErr, rc, "failed to open '$(path)', it is not a csra-database", "path=%s", path ) );
            } else {
                prepare_ctx prep;   /* from cmdline_cmn.h */

                prep . omit_qualities = ctx -> options -> cmn . omit_qualities;
                prep . read_tlen = ctx -> options -> read_tlen;
                prep . use_primary_alignments = ( ( ctx -> options -> cmn . tab_select & primary_ats ) == primary_ats );
                prep . use_secondary_alignments = ( ( ctx -> options -> cmn . tab_select & secondary_ats ) == secondary_ats );
                prep . use_evidence_alignments = ( ( ctx -> options -> cmn . tab_select & evidence_ats ) == evidence_ats );
                prep . ref_iter = ctx -> ref_iter;
                prep . spot_group = spot_group;
                prep . on_section = prepare_section_cb;
                prep . data = ctx -> cursor_ids;
                prep . path = path;
                prep . db = NULL;
                prep . prim_cur = NULL;
                prep . sec_cur = NULL;
                prep . ev_cur = NULL;

                rc = prepare_ref_iter( &prep, ctx -> vdb_mgr, ctx -> vdb_schema, path, ctx -> ranges ); /* cmdline_cmn.c */
                if ( rc == 0 && prep . db == NULL ) {
                    rc = RC ( rcApp, rcNoTarg, rcOpening, rcSelf, rcInvalid );
                    LOGERR( klogInt, rc, "unsupported source" );
                }
                if ( prep . prim_cur != NULL ) { VCursorRelease( prep.prim_cur ); }
                if ( prep . sec_cur != NULL ) { VCursorRelease( prep.sec_cur ); }
                if ( prep . ev_cur != NULL ) { VCursorRelease( prep.ev_cur ); }
            }
        }
    }
    return rc;
}


/* free all cursor-ids-blocks created in parallel with the alignment-cursor */
static void CC cur_id_vector_entry_whack( void *item, void *data ) {
    pileup_col_ids * ids = item;
    free( ids );
}

static rc_t pileup_main( Args * args, pileup_options *options ) {
    foreach_arg_ctx arg_ctx;
    pileup_callback_data cb_data;
    KDirectory * dir = NULL;
    Vector cur_ids_vector;

    /* (1) make the align-manager ( necessary to make a ReferenceIterator... ) */
    rc_t rc = AlignMgrMakeRead ( &cb_data.almgr );
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "AlignMgrMake() failed" );
    }

    VectorInit ( &cur_ids_vector, 0, 20 );
    cb_data . options = options;
    arg_ctx . options = options;
    arg_ctx . vdb_schema = NULL;
    arg_ctx . cursor_ids = &cur_ids_vector;

    /* (2) make the reference-iterator */
    if ( rc == 0 ) {
        PlacementRecordExtendFuncs cb_block;

        cb_block.data = &cb_data;
        cb_block.destroy = NULL;
        cb_block.populate = populate_tooldata;
        cb_block.alloc_size = alloc_size;
        cb_block.fixed_size = 0;

        rc = AlignMgrMakeReferenceIterator ( cb_data . almgr, &( arg_ctx . ref_iter ), &cb_block, options -> minmapq );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "AlignMgrMakeReferenceIterator() failed" );
        }
    }

    /* (3) make a KDirectory ( necessary to make a vdb-manager ) */
    if ( rc == 0 ) {
        rc = KDirectoryNativeDir( &dir );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "KDirectoryNativeDir() failed" );
        }
    }

    /* (4) make a vdb-manager */
    if ( rc == 0 ) {
        rc = VDBManagerMakeRead ( &( arg_ctx . vdb_mgr ), dir );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "VDBManagerMakeRead() failed" );
        } else {
            ReportSetVDBManager( arg_ctx.vdb_mgr );
        }
    }

    if ( rc == 0 && options -> cmn . no_mt )
    {
        rc = VDBManagerDisablePagemapThread ( arg_ctx . vdb_mgr );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "VDBManagerDisablePagemapThread() failed" );
        }
    }

    /* (5) make a vdb-schema */
    if ( rc == 0 ) {
        rc = VDBManagerMakeSRASchema( arg_ctx . vdb_mgr, &( arg_ctx . vdb_schema ) );
        if ( rc != 0 ) {
            LOGERR( klogInt, rc, "VDBManagerMakeSRASchema() failed" );
        } else if ( options -> cmn . schema_file != NULL ) {
            rc = VSchemaParseFile( arg_ctx . vdb_schema, "%s", options -> cmn . schema_file );
            if ( rc != 0 ) {
                LOGERR( klogInt, rc, "VSchemaParseFile() failed" );
            }
        }
    }

    if ( rc == 0 ) {
        switch( options -> function ) {
            case sra_pileup_counters    : options -> cmn . omit_qualities = true;
                                          options -> read_tlen = false;
                                          break;

            case sra_pileup_stat        : options -> cmn . omit_qualities = true;
                                          options -> read_tlen = true;
                                          break;

            case sra_pileup_debug       : options -> cmn . omit_qualities = true;
                                          options -> read_tlen = true;
                                          break;

            case sra_pileup_samtools    : options -> read_tlen = false;
                                          break;

            case sra_pileup_mismatch    : options -> cmn . omit_qualities = true;
                                          options -> read_tlen = false;
                                          break;

            case sra_pileup_index      :  options -> cmn . omit_qualities = true;
                                          options -> read_tlen = false;
                                          break;

            case sra_pileup_varcount   :  options -> cmn . omit_qualities = true;
                                          options -> read_tlen = false;
                                          break;
        }
    }

    /* (5) loop through the given input-filenames and load the ref-iter with it's input */
    if ( rc == 0 ) {
        BSTree regions;
        rc = init_ref_regions( &regions, args ); /* cmdline_cmn.c */
        if ( rc == 0 ) {
            bool empty = false;

            check_ref_regions( &regions, options -> merge_dist ); /* sanitize input, merge slices... */
            options -> skiplist = skiplist_make( &regions ); /* create skiplist for neighboring slices */

            arg_ctx . ranges = &regions;
            rc = foreach_argument( args, dir, options -> div_by_spotgrp, &empty, on_argument, &arg_ctx ); /* cmdline_cmn.c */
            if ( empty ) {
                Usage ( args );
                rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcInsufficient );
            }
            free_ref_regions( &regions );
        }
    }

    /* (6) walk the "loaded" ref-iterator ===> perform the pileup */
    if ( rc == 0 ) {
        /* ============================================== */
        switch( options -> function )
        {
            case sra_pileup_stat        : rc = walk_stat( arg_ctx . ref_iter, options ); break;
            case sra_pileup_counters    : rc = walk_counters( arg_ctx . ref_iter, options ); break;
            case sra_pileup_debug       : rc = walk_debug( arg_ctx . ref_iter, options ); break;
            case sra_pileup_mismatch    : rc = walk_mismatches( arg_ctx . ref_iter, options ); break;
            case sra_pileup_index       : rc = walk_index( arg_ctx . ref_iter, options ); break;
            case sra_pileup_varcount    : rc = walk_varcount( arg_ctx . ref_iter, options ); break;
			case sra_pileup_indels      : rc = walk_indels( arg_ctx . ref_iter, options ); break;
            default : rc = walk_ref_iter( arg_ctx . ref_iter, options ); break;
        }
        /* ============================================== */
    }

    if ( arg_ctx . vdb_mgr != NULL ) { VDBManagerRelease( arg_ctx . vdb_mgr ); }
    if ( arg_ctx . vdb_schema != NULL ) { VSchemaRelease( arg_ctx . vdb_schema ); }
    if ( dir != NULL ) { KDirectoryRelease( dir ); }
    if ( arg_ctx . ref_iter != NULL ) { ReferenceIteratorRelease( arg_ctx . ref_iter ); }
    if ( cb_data . almgr != NULL ) { AlignMgrRelease ( cb_data . almgr ); }
    VectorWhack ( &cur_ids_vector, cur_id_vector_entry_whack, NULL );

    return rc;
}

/* =========================================================================================== */

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = KOutHandlerSet( write_to_FILE, stdout );
    ReportBuildDate( __DATE__ );
    if ( rc != 0 ) {
        LOGERR( klogInt, rc, "KOutHandlerSet() failed" );
    } else {
        Args * args;

        /* KLogHandlerSetStdErr(); */
        rc = ArgsMakeAndHandle( &args, argc, argv, 2,
            MyOptions, sizeof MyOptions / sizeof MyOptions [ 0 ],
            CommonOptions_ptr(), CommonOptions_count() );
        if ( rc == 0 ) {
            rc = parse_inf_file( args ); /* cmdline_cmn.h */
            if ( rc == 0 ) {
                pileup_options options;
                rc = get_pileup_options( args, &options );
                if ( rc == 0 ) {
                    out_redir redir; /* from out_redir.h */
                    enum out_redir_mode mode;

                    options . skiplist = NULL;

                    if ( options . cmn . gzip_output ) {
                        mode = orm_gzip;
                    } else if ( options . cmn . bzip_output ) {
                        mode = orm_bzip2;
                    } else {
                        mode = orm_uncompressed;
                    }

                    rc = init_out_redir( &redir, mode, options.cmn.output_file, 32 * 1024 ); /* from out_redir.c */

                    /*
                    if ( options . cmn . output_file != NULL ) {
                        rc = set_stdout_to( options . cmn . gzip_output,
                                            options . cmn . bzip_output,
                                            options . cmn . output_file,
                                            32 * 1024 );
                    }
                    */

                    if ( rc == 0 ) {
                        if ( options . function == sra_pileup_report_ref ||
                             options . function == sra_pileup_report_ref_ext ) {
                            rc = report_on_reference( args, options . function == sra_pileup_report_ref_ext ); /* reref.c */
                        } else if ( options . function == sra_pileup_deletes ) {
                            rc = report_deletes( args, 10 ); /* see above */
                        } else if ( options . function == sra_pileup_test ) {
                            rc = pileup_v2( args, &options ); /* see above */
                        } else {
                            /* ============================== */
                            rc = pileup_main( args, &options );
                            /* ============================== */
                        }
                    }

                    /* if ( options . cmn . output_file != NULL ) { release_stdout_redirection(); } */

                    release_out_redir( &redir ); /* from out_redir.c */

                    if ( options . skiplist != NULL ) {
                        skiplist_release( options . skiplist );
                    }
                }
            }
            ArgsWhack( args );
        }
    }

    {
        /* Report execution environment if necessary */
        rc_t rc2 = ReportFinalize( rc );
        if ( rc == 0 ) { rc = rc2; }
    }
    return VDB_TERMINATE( rc );
}
