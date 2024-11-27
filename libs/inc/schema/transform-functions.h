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
*
* Purpose: Declare external (and internal) transform functions
* Used by: schema/SraSchema.cpp
*
* How to add a new transform function to SRA:
*  Add a VTRANSFACT_DECL for the new function
*  Add the new function to VLinkerIntFactory sra_fact[]
*
*  Copy/paste is your friend
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <vdb/xform.h>
#include <vdb/vdb-priv.h>

extern VTRANSFACT_DECL ( sra_hello );

/* originally declared in ncbi-vdb/libs/vdb/transform-functions.h */
// extern VTRANSFACT_DECL ( ALIGN_align_restore_read );
// extern VTRANSFACT_DECL ( ALIGN_cigar );
// extern VTRANSFACT_DECL ( ALIGN_cigar_2 );
// extern VTRANSFACT_DECL ( ALIGN_generate_has_mismatch );
// extern VTRANSFACT_DECL ( ALIGN_generate_mismatch );
// extern VTRANSFACT_DECL ( ALIGN_generate_mismatch_qual );
// extern VTRANSFACT_DECL ( ALIGN_project_from_sequence );
// extern VTRANSFACT_DECL ( ALIGN_raw_restore_read );
// extern VTRANSFACT_DECL ( ALIGN_ref_restore_read );
// extern VTRANSFACT_DECL ( ALIGN_ref_sub_select );
// extern VTRANSFACT_DECL ( ALIGN_seq_restore_read );
// extern VTRANSFACT_DECL ( ALIGN_seq_restore_linkage_group );
// extern VTRANSFACT_DECL ( INSDC_SEQ_rand_4na_2na );
// extern VTRANSFACT_DECL ( INSDC_SRA_format_spot_name );
// extern VTRANSFACT_DECL ( INSDC_SRA_format_spot_name_no_coord );
// extern VTRANSFACT_DECL ( INSDC_SRA_read2spot_filter );
// extern VTRANSFACT_DECL ( INSDC_SRA_spot2read_filter );
// extern VTRANSFACT_DECL ( NCBI_SRA_ABI_tokenize_spot_name );
// extern VTRANSFACT_DECL ( NCBI_SRA_Helicos_tokenize_spot_name );
// extern VTRANSFACT_DECL ( NCBI_SRA_Illumina_tokenize_spot_name );
// extern VTRANSFACT_DECL ( NCBI_SRA_IonTorrent_tokenize_spot_name );
// extern VTRANSFACT_DECL ( NCBI_SRA_GenericFastq_tokenize_spot_name );
// extern VTRANSFACT_DECL ( NCBI_SRA__454__dynamic_read_desc );
// extern VTRANSFACT_DECL ( NCBI_SRA__454__process_position );
// extern VTRANSFACT_DECL ( NCBI_SRA__454__tokenize_spot_name );
// extern VTRANSFACT_DECL ( NCBI_SRA_bio_start );
// extern VTRANSFACT_DECL ( NCBI_SRA_bio_end );
// extern VTRANSFACT_DECL ( NCBI_SRA_decode_CLIP );
// extern VTRANSFACT_DECL ( NCBI_SRA_decode_INTENSITY );
// extern VTRANSFACT_DECL ( NCBI_SRA_decode_NOISE );
// extern VTRANSFACT_DECL ( NCBI_SRA_decode_POSITION );
// extern VTRANSFACT_DECL ( NCBI_SRA_decode_QUALITY );
// extern VTRANSFACT_DECL ( NCBI_SRA_decode_READ );
// extern VTRANSFACT_DECL ( NCBI_SRA_decode_SIGNAL );
// extern VTRANSFACT_DECL ( NCBI_SRA_denormalize );
// extern VTRANSFACT_DECL ( NCBI_SRA_extract_coordinates );
// extern VTRANSFACT_DECL ( NCBI_SRA_extract_name_coord );
// extern VTRANSFACT_DECL ( NCBI_SRA_fix_read_seg );
// extern VTRANSFACT_DECL ( NCBI_SRA_linker_from_readn );
// extern VTRANSFACT_DECL ( NCBI_SRA_lookup );
// extern VTRANSFACT_DECL ( NCBI_SRA_make_position );
// extern VTRANSFACT_DECL ( NCBI_SRA_make_read_desc );
// extern VTRANSFACT_DECL ( NCBI_SRA_make_spot_desc );
// extern VTRANSFACT_DECL ( NCBI_SRA_make_spot_filter );
// extern VTRANSFACT_DECL ( NCBI_SRA_normalize );

// #if HAVE_PREFIX_TREE_TO_NAME
// extern VTRANSFACT_DECL ( NCBI_SRA_prefix_tree_to_name );
// #endif

// extern VTRANSFACT_DECL ( NCBI_SRA_qual4_decode );
// extern VTRANSFACT_DECL ( NCBI_SRA_qual4_decompress_v1 );

// #if HAVE_READ_LEN_FROM_NREADS
// extern VTRANSFACT_DECL ( NCBI_SRA_read_len_from_nreads );
// extern VTRANSFACT_DECL ( NCBI_SRA_read_start_from_nreads );
// #endif

// extern VTRANSFACT_DECL ( NCBI_SRA_read_seg_from_readn );
// extern VTRANSFACT_DECL ( NCBI_SRA_rewrite_spot_name );
// extern VTRANSFACT_DECL ( NCBI_SRA_rotate );
// extern VTRANSFACT_DECL ( NCBI_SRA_swap );
// extern VTRANSFACT_DECL ( NCBI_SRA_syn_quality );
// extern VTRANSFACT_DECL ( NCBI_SRA_syn_quality_read );

// extern VTRANSFACT_DECL ( NCBI_WGS_build_read_type );
// extern VTRANSFACT_DECL ( NCBI_WGS_build_scaffold_qual );
// extern VTRANSFACT_DECL ( NCBI_WGS_build_scaffold_read );
// extern VTRANSFACT_DECL ( NCBI_WGS_tokenize_nuc_accession );
// extern VTRANSFACT_DECL ( NCBI_WGS_tokenize_prot_accession );

// extern VTRANSFACT_DECL ( NCBI_align_clip );
// extern VTRANSFACT_DECL ( NCBI_align_clip_2 );
// extern VTRANSFACT_DECL ( NCBI_align_compress_quality );
// extern VTRANSFACT_DECL ( NCBI_align_decompress_quality );
// extern VTRANSFACT_DECL ( NCBI_align_edit_distance );
// extern VTRANSFACT_DECL ( NCBI_align_edit_distance_2 );
// extern VTRANSFACT_DECL ( NCBI_align_edit_distance_3 );
// extern VTRANSFACT_DECL ( NCBI_align_generate_mismatch_qual_2 );
// extern VTRANSFACT_DECL ( NCBI_align_generate_preserve_qual );
// extern VTRANSFACT_DECL ( NCBI_align_get_clipped_cigar );
// extern VTRANSFACT_DECL ( NCBI_align_get_clipped_cigar_2 );
// extern VTRANSFACT_DECL ( NCBI_align_get_clipped_ref_offset );
// extern VTRANSFACT_DECL ( NCBI_align_get_left_soft_clip );
// extern VTRANSFACT_DECL ( NCBI_align_get_left_soft_clip_2 );
// extern VTRANSFACT_DECL ( NCBI_align_get_mate_align_id );
// extern VTRANSFACT_DECL ( NCBI_align_get_mismatch_read );
// extern VTRANSFACT_DECL ( NCBI_align_get_ref_delete );
// extern VTRANSFACT_DECL ( NCBI_align_get_ref_insert );
// extern VTRANSFACT_DECL ( NCBI_align_get_ref_len );
// extern VTRANSFACT_DECL ( NCBI_align_get_ref_len_2 );
// extern VTRANSFACT_DECL ( NCBI_align_get_ref_mismatch );
// extern VTRANSFACT_DECL ( NCBI_align_get_ref_preserve_qual );
// extern VTRANSFACT_DECL ( NCBI_align_get_right_soft_clip );
// extern VTRANSFACT_DECL ( NCBI_align_get_right_soft_clip_2 );
// extern VTRANSFACT_DECL ( NCBI_align_get_right_soft_clip_3 );
// extern VTRANSFACT_DECL ( NCBI_align_get_right_soft_clip_4 );
// extern VTRANSFACT_DECL ( NCBI_align_get_right_soft_clip_5 );
// extern VTRANSFACT_DECL ( NCBI_align_get_sam_flags );
// extern VTRANSFACT_DECL ( NCBI_align_get_sam_flags_2 );
// extern VTRANSFACT_DECL ( NCBI_align_get_seq_preserve_qual );
// extern VTRANSFACT_DECL ( NCBI_align_local_ref_id );
// extern VTRANSFACT_DECL ( NCBI_align_local_ref_start );
// extern VTRANSFACT_DECL ( NCBI_align_make_cmp_read_desc );
// extern VTRANSFACT_DECL ( NCBI_align_make_read_start );
// extern VTRANSFACT_DECL ( NCBI_align_mismatch_restore_qual );
// extern VTRANSFACT_DECL ( NCBI_align_not_my_row );
// extern VTRANSFACT_DECL ( NCBI_align_raw_restore_qual );
// extern VTRANSFACT_DECL ( NCBI_align_ref_name );
// extern VTRANSFACT_DECL ( NCBI_align_ref_pos );
// extern VTRANSFACT_DECL ( NCBI_align_ref_seq_id );
// extern VTRANSFACT_DECL ( NCBI_align_ref_sub_select_preserve_qual );
// extern VTRANSFACT_DECL ( NCBI_align_rna_orientation );
// extern VTRANSFACT_DECL ( NCBI_align_seq_construct_read );
// extern VTRANSFACT_DECL ( NCBI_align_template_len );
// extern VTRANSFACT_DECL ( NCBI_color_from_dna );
// extern VTRANSFACT_DECL ( NCBI_dna_from_color );
// extern VTRANSFACT_DECL ( NCBI_SRA_useRnaFlag );
// extern VTRANSFACT_DECL ( NCBI_fp_extend );
// extern VTRANSFACT_DECL ( NCBI_lower_case_tech_reads );
// extern VTRANSFACT_DECL ( NCBI_unpack );
// extern VTRANSFACT_DECL ( NCBI_unzip );
// extern VTRANSFACT_DECL ( NCBI_var_tokenize_var_id );

// extern bool CC NCBI_SRA_accept_untyped ( struct KTable const *tbl, struct KMetadata const *meta );
// extern bool CC NCBI_SRA__454__untyped_0 ( struct KTable const *tbl, struct KMetadata const *meta );
// extern bool CC NCBI_SRA__454__untyped_1_2a ( struct KTable const *tbl, struct KMetadata const *meta );
// extern bool CC NCBI_SRA__454__untyped_1_2b ( struct KTable const *tbl, struct KMetadata const *meta );
// extern bool CC NCBI_SRA_Illumina_untyped_0a ( struct KTable const *tbl, struct KMetadata const *meta );
// extern bool CC NCBI_SRA_Illumina_untyped_0b ( struct KTable const *tbl, struct KMetadata const *meta );
// extern bool CC NCBI_SRA_Illumina_untyped_1a ( struct KTable const *tbl, struct KMetadata const *meta );
// extern bool CC NCBI_SRA_Illumina_untyped_1b ( struct KTable const *tbl, struct KMetadata const *meta );
// extern bool CC NCBI_SRA_ABI_untyped_1 ( struct KTable const *tbl, struct KMetadata const *meta );

rc_t SraLinkSchema ( const VDBManager * );

#ifdef __cplusplus
}
#endif