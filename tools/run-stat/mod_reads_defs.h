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

#ifndef _h_stat_mod_reads_defs_
#define _h_stat_mod_reads_defs_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include "rd_filter.h"

#define MAX_COMPRESSED_BASE_POS 49
#define MAX_BASE_POS 1899

#define MIN_QUALITY 2
#define MAX_QUALITY 40

#define IDX_A 0
#define IDX_C 1
#define IDX_G 2
#define IDX_T 3
#define IDX_N 4

typedef struct base_pos
{
    uint32_t from;
    uint32_t to;

    uint64_t read_count;
    uint64_t count;

    uint64_t qual[ MAX_QUALITY + 1 ];    /* qual 0...MAX_QUALITY */
    double mean;
    double median;
    uint8_t mode;
    double upper_quart;
    double lower_quart;
    double upper_centile;
    double lower_centile;

    uint64_t base_sum[ IDX_N + 1 ];
    double base_percent[ IDX_N + 1 ];
} base_pos;
typedef base_pos* p_base_pos;


#define NKMER5 1024


typedef struct kmer5_count
{
    uint64_t count[ MAX_COMPRESSED_BASE_POS + 1 ];
    double bp_expected[ MAX_COMPRESSED_BASE_POS + 1 ];
    double bp_obs_vs_exp[ MAX_COMPRESSED_BASE_POS + 1 ];
    uint64_t total_count;
    double probability;
    double expected;
    double observed_vs_expected;
    double max_bp_obs_vs_exp;
    uint16_t max_bp_obs_vs_exp_at;
} kmer5_count;
typedef kmer5_count* p_kmer5_count;


typedef struct reads_data
{
    /* the statistical data to be collected */
    uint64_t sum_reads;
    uint64_t min_read_len;
    uint64_t max_read_len;
    double mean_read_len;

    uint8_t min_quality;
    uint8_t max_quality;

    /* for the whole sequence */
    base_pos total;
    double base_prob[ IDX_N + 1 ];

    /* for a specific base-position */
    base_pos bp_data[ MAX_COMPRESSED_BASE_POS + 1 ];

    /* to count the Kmer's */
    uint64_t bp_total_kmers[ MAX_COMPRESSED_BASE_POS + 1 ];
    kmer5_count kmer5[ NKMER5 ];
    uint16_t kmer5_idx[ NKMER5 ];
    uint64_t total_kmers;

    /* the filtered read-data */
    rd_filter filter;

    /* flags how to process the data */
    bool bio;
    bool trim;
    bool cut;

} reads_data;
typedef reads_data* p_reads_data;


#endif
