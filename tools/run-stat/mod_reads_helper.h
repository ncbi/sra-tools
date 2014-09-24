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

#ifndef _h_stat_mod_reads_helper_
#define _h_stat_mod_reads_helper_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <klib/rc.h>
#include "mod_reads_defs.h"

uint8_t compress_base_pos( const uint32_t src );
uint32_t compress_start_lookup( const uint8_t idx );
uint32_t compress_end_lookup( const uint8_t idx );

uint16_t kmer_ascii_2_int( char * s );
void kmer_int_2_ascii( const uint16_t kmer, char * s );
uint16_t kmer_add_base( const uint16_t kmer, const char c );

void setup_bp_array( p_base_pos bp, const uint8_t count );
double kmer_probability( const uint16_t kmer, double * base_prob );
void count_base( p_base_pos bp, const char base, const uint8_t quality );
uint16_t count_kmer( p_reads_data data,
                     const uint16_t kmer,
                     const uint8_t base_pos, 
                     const char base );

void calculate_quality_mean_median_quart_centile( p_base_pos bp );
void calculate_bases_percentage( p_base_pos bp );
void calculate_base_probability( p_reads_data data );
void calculate_kmer_observed_vs_expected( p_reads_data data );

#endif
