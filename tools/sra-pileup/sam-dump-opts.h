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

#ifndef _h_sam_dump_opts_
#define _h_sam_dump_opts_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <klib/container.h>
#include <klib/vector.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <klib/namelist.h>

#include <kapp/args.h>
#include "perf_log.h"
#include "rna_splice_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <strtol.h>


#define OPT_UNALIGNED   "unaligned"
#define OPT_PRIM_ONLY   "primary"
#define OPT_CIGAR_LONG  "cigar-long"
#define OPT_CG_SAM      "CG-SAM"
#define OPT_CG_EVIDENCE "CG-evidence"
#define OPT_CG_EV_DNB   "CG-ev-dnb"
#define OPT_CG_MAPP     "CG-mappings"
#define OPT_REGION      "aligned-region"
#define OPT_RECAL_HDR   "header"
#define OPT_HDR_FILE    "header-file"
#define OPT_NO_HDR      "no-header"
#define OPT_USE_SEQID   "seqid"
#define OPT_HIDE_IDENT  "hide-identical"
#define OPT_CIGAR_CG    "cigar-CG"
#define OPT_CIGAR_CG_M  "cigar-CG-merge"
#define OPT_PREFIX      "prefix"
#define OPT_REVERSE     "reverse"
#define OPT_SPOTGRP     "spot-group"
#define OPT_MATE_GAP    "mate-cache-row-gap"
#define OPT_XI_DEBUG    "XI"
#define OPT_Q_QUANT     "qual-quant"
#define OPT_GZIP        "gzip"
#define OPT_BZIP2       "bzip2"
#define OPT_FASTQ       "fastq"
#define OPT_FASTA       "fasta"
#define OPT_HDR_COMMENT "header-comment"
#define OPT_MATE_DIST   "matepair-distance"
#define OPT_OUTPUTFILE  "output-file"
#define OPT_OUTBUFSIZE  "output-buffer-size"
#define OPT_REPORT      "report"
#define OPT_CACHEREPORT "cachereport"
#define OPT_UNALIGNED_ONLY "unaligned-spots-only"
#define OPT_CG_NAMES    "CG-names"
#define OPT_CIGAR_TEST  "cigar-test"
#define OPT_CURSOR_CACHE "cursor-cache"
#define OPT_DUMP_MODE   "dump-mode"
#define OPT_MIN_MAPQ    "min-mapq"
#define OPT_NO_MATE_CACHE "no-mate-cache"
#define OPT_LEGACY      "legacy"
#define OPT_NEW         "new"
#define OPT_RNA_SPLICE  "rna-splicing"
#define OPT_RNA_SPLICEL "rna-splice-level"
#define OPT_RNA_SPLICE_LOG "rna-splice-log"
#define OPT_NO_MT       "disable-multithreading"
#define OPT_TIMING      "timing"

typedef struct range
{
    uint64_t start;
    uint64_t end;
} range;


typedef struct reference_region
{
    BSTNode node;
    const char * name;      /* the name of the reference */
    Vector ranges;          /* what regions on this reference */
} reference_region;


enum header_mode
{
    hm_none = 0,    /* do not dump the headers at all */
    hm_recalc,      /* recalculate the headers */
    hm_dump,        /* dump the header found in metadata */
    hm_file         /* take the complete header part from a file */
};

enum output_format
{
    of_sam = 0,     /* use sam-tools format */
    of_fasta,       /* use fasta-format */
    of_fastq        /* use fastq-format */
};

enum output_compression
{
    oc_none = 0,    /* do not compress output */
    oc_gzip,        /* compress output with gzip */
    oc_bzip2        /* compress output with bzip2 */
};

enum cigar_treatment
{
    ct_unchanged = 0,   /* use the cigar-string as it is stored */
    ct_cg_style,        /* transform cigar into cg-style ( has B/N ) */
    ct_cg_merge         /* transform cg-data(length of read/patterns in cigar) into valid SAM (cigar/READ/QUALITY) */
};


enum dump_mode
{
    /* in case of: aligned reads requested + no regions given */
    dm_one_ref_at_a_time = 0,   /* create a set-iter each for every reference sequentially, put only one reference into it */
    dm_prepare_all_refs         /* create only ONE set-iter, put ALL references into it */
};


typedef struct samdump_opts
{
    /* tree with regions, each node has a sorted vector of ranges, can be empty ... */
    BSTree regions;     /* contains reference_region structs */

    /* vector with header-comments, can be empty... */
    VNamelist * hdr_comments;

    /* vector input files/accessions/url's */
    VNamelist * input_files;

    /* vector with metapair-distances... */
    Vector mp_dist;

    /* prepend qname with this prefix */
    const char * qname_prefix;

    /* the quality quantization string */
    const char * qual_quant;

    /* optional outputfile */
    const char * outputfile;

    /* optional header-file */
    const char * header_file;

    /* cigar-test >>> not advertized! */
    const char * cigar_test;

    /* timing-file >>> not advertized! */
    const char * timing_file;

    /* log file for rna-splicing-events */
    const char * rna_splice_log_file;

    /* timing-performane-log, created if timing_file given */
    struct perf_log * perf_log;

    /* logging of rna-splicing on reqest */
    struct rna_splice_log * rna_splice_log;

    uint32_t region_count;
    uint32_t input_file_count;
    uint32_t rna_splice_level;  /* can be 0 || 1 || 2 */

    int32_t min_mapq;

    /* how much buffering on the output-buffer, of OFF if zero */
    uint32_t output_buffer_size;

    /* mate's farther apart than this are not cached */
    uint32_t mape_gap_cache_limit;

    size_t cursor_cache_size;

    /* how the sam-headers are treated */
    enum header_mode header_mode;

    /* how the cigar-string is treated */
    enum cigar_treatment cigar_treatment;

    /* in which format should the output be created */
    enum output_format output_format;

    /* should the output be compressed / in which format */
    enum output_compression output_compression;

    /* how to process in case of: aligned reads requested + no regions given */
    enum dump_mode dump_mode;

    /* use a mate-cache to dump aligned and half-aligned reads */
    bool use_mate_cache;
    bool force_legacy;
    bool force_new;

    /* which tables have to be processed/dumped */
    bool dump_primary_alignments;
    bool dump_secondary_alignments;
    bool dump_cg_evidence;
    bool dump_cg_sam;
    bool dump_cg_ev_dnb;
    bool merge_cg_cigar;

    bool dump_unaligned_reads;
    bool dump_unaligned_only;
    bool dump_cga_tools_mode;

    /* what alignment/unaligned reads should be dumped */
    bool print_half_unaligned_reads;
    bool print_fully_unaligned_reads;

    /* flag that shows if we need to filter by matepair-distance */
    bool use_matepair_filter;
    bool use_min_mapq;

    /* options changing the output-format */
    bool use_seqid_as_refname;
    bool use_long_cigar;
    bool print_matches_as_equal_sign;
    bool print_spot_group_in_name;
    bool reverse_unaligned_reads;
    bool print_alignment_id_in_column_xi;
    bool report_options;
    bool report_cache;
    bool print_cg_names;
    bool rna_splicing;

    /* option to disable multi-threading */
    bool no_mt;
    
    uint8_t qual_quant_matrix[ 256 ];
} samdump_opts;


typedef struct foreach_reference_func
{
    rc_t ( CC * on_reference ) ( const char * name, Vector *ranges, void *data );
    const char * name;
    void * data;
    rc_t rc;
} foreach_reference_func;


rc_t foreach_reference( BSTree * regions,
    rc_t ( CC * on_reference ) ( const char * name, Vector *ranges, void *data ), 
    void *data );

int cmp_pchar( const char * a, const char * b );

rc_t gather_options( Args * args, samdump_opts * opts );
void report_options( const samdump_opts * opts );
void release_options( samdump_opts * opts );

bool filter_by_matepair_dist( const samdump_opts * opts, int32_t tlen );

bool is_this_alignment_requested( const samdump_opts * opts, const char *refname, uint32_t refname_len,
                                  uint64_t start, uint64_t len );

rc_t dump_name( const samdump_opts * opts, int64_t seq_spot_id,
                const char * spot_group, uint32_t spot_group_len );

rc_t dump_name_legacy( const samdump_opts * opts, const char * name, size_t name_len,
                       const char * spot_group, uint32_t spot_group_len );

rc_t dump_quality( const samdump_opts * opts, char const *quality, uint32_t qual_len, bool reverse );

rc_t dump_quality_33( const samdump_opts * opts, char const *quality, uint32_t qual_len, bool reverse );

#endif
