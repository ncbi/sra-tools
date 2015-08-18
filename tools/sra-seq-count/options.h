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

#ifndef _h_sra_seq_count_options_
#define _h_sra_seq_count_options_

#include <os-native.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SSC_MODE_NORMAL	0
#define SSC_MODE_DEBUG	1

#define SSC_COMPARE_FT_OUTER_VS_ALIGN_OUTER		0
#define SSC_COMPARE_FT_RANGES_VS_ALIGN_OUTER	1
#define SSC_COMPARE_FT_RANGES_VS_ALIGN_RANGES	2

struct sra_seq_count_options
{
    const char * sra_accession;
    const char * gtf_file;
    const char * id_attrib;
    const char * feature_type;
	const char * ref_trans;
	const char * function;
	const char * refs;
	int max_genes;
	int output_mode;
	int compare_mode;
	int mapq;
	bool valid;
	bool silent;
};

/* the main/default function */
int perform_seq_counting_1( const struct sra_seq_count_options * options );
int perform_seq_counting_2( const struct sra_seq_count_options * options );

/* alternativie function : list the reference-names in the gtf file... */
void list_gtf_refs( const struct sra_seq_count_options * options );
void list_acc_refs( const struct sra_seq_count_options * options );

bool is_gtf_sorted( const struct sra_seq_count_options * options );
bool is_acc_sorted( const struct sra_seq_count_options * options );

void token_tests( const struct sra_seq_count_options * options );
void index_gtf( const struct sra_seq_count_options * options );

#ifdef __cplusplus
}
#endif


#endif
