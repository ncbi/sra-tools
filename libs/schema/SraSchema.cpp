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
*/

#include <schema/transform-functions.h>

#include <bitstr.h>

#include <klib/data-buffer.h>
#include <klib/rc.h>

static VLinkerIntFactory sra_fact [] =
{
//     { ALIGN_align_restore_read, "ALIGN:align_restore_read" },
//     { ALIGN_cigar, "ALIGN:cigar" },
//     { ALIGN_cigar_2, "ALIGN:cigar_2" },
//     { ALIGN_generate_has_mismatch, "ALIGN:generate_has_mismatch" },
//     { ALIGN_generate_mismatch, "ALIGN:generate_mismatch" },
//     { ALIGN_generate_mismatch_qual, "ALIGN:generate_mismatch_qual" },
//     { ALIGN_project_from_sequence, "ALIGN:project_from_sequence" },
//     { ALIGN_raw_restore_read, "ALIGN:raw_restore_read" },
//     { ALIGN_ref_restore_read, "ALIGN:ref_restore_read" },
//     { ALIGN_ref_sub_select, "ALIGN:ref_sub_select" },
//     { ALIGN_seq_restore_read, "ALIGN:seq_restore_read" },
//     { ALIGN_seq_restore_linkage_group, "ALIGN:seq_restore_linkage_group" },
//     { INSDC_SEQ_rand_4na_2na, "INSDC:SEQ:rand_4na_2na" },
//     { INSDC_SRA_format_spot_name, "INSDC:SRA:format_spot_name" },
//     { INSDC_SRA_format_spot_name_no_coord, "INSDC:SRA:format_spot_name_no_coord" },
//     { INSDC_SRA_read2spot_filter, "INSDC:SRA:read2spot_filter" },
//     { INSDC_SRA_spot2read_filter, "INSDC:SRA:spot2read_filter" },
//     { NCBI_SRA_ABI_tokenize_spot_name, "NCBI:SRA:ABI:tokenize_spot_name" },
//     { NCBI_SRA_Helicos_tokenize_spot_name, "NCBI:SRA:Helicos:tokenize_spot_name" },
//     { NCBI_SRA_Illumina_tokenize_spot_name, "NCBI:SRA:Illumina:tokenize_spot_name" },
//     { NCBI_SRA_IonTorrent_tokenize_spot_name, "NCBI:SRA:IonTorrent:tokenize_spot_name" },
//     { NCBI_SRA_GenericFastq_tokenize_spot_name, "NCBI:SRA:GenericFastq:tokenize_spot_name" },
//     { NCBI_SRA__454__dynamic_read_desc, "NCBI:SRA:_454_:dynamic_read_desc" },
//     { NCBI_SRA__454__process_position, "NCBI:SRA:_454_:process_position" },
//     { NCBI_SRA__454__tokenize_spot_name, "NCBI:SRA:_454_:tokenize_spot_name" },
//     { NCBI_SRA_bio_start, "NCBI:SRA:bio_start" },
//     { NCBI_SRA_bio_end, "NCBI:SRA:bio_end" },
//     { NCBI_SRA_decode_CLIP, "NCBI:SRA:decode:CLIP" },
//     { NCBI_SRA_decode_INTENSITY, "NCBI:SRA:decode:INTENSITY" },
//     { NCBI_SRA_decode_NOISE, "NCBI:SRA:decode:NOISE" },
//     { NCBI_SRA_decode_POSITION, "NCBI:SRA:decode:POSITION" },
//     { NCBI_SRA_decode_QUALITY, "NCBI:SRA:decode:QUALITY" },
//     { NCBI_SRA_decode_READ, "NCBI:SRA:decode:READ" },
//     { NCBI_SRA_decode_SIGNAL, "NCBI:SRA:decode:SIGNAL" },
//     { NCBI_SRA_denormalize, "NCBI:SRA:denormalize" },
//     { NCBI_SRA_extract_coordinates, "NCBI:SRA:extract_coordinates" },
//     { NCBI_SRA_extract_name_coord, "NCBI:SRA:extract_name_coord" },
//     { NCBI_SRA_fix_read_seg, "NCBI:SRA:fix_read_seg" },
// #if HAVE_LINKER_FROM_READN
//     { NCBI_SRA_linker_from_readn, "NCBI:SRA:linker_from_readn" },
// #endif
//     { NCBI_SRA_lookup, "NCBI:SRA:lookup" },
//     { NCBI_SRA_make_position, "NCBI:SRA:make_position" },
//     { NCBI_SRA_make_read_desc, "NCBI:SRA:make_read_desc" },
//     { NCBI_SRA_make_spot_desc, "NCBI:SRA:make_spot_desc" },
//     { NCBI_SRA_make_spot_filter, "NCBI:SRA:make_spot_filter" },
//     { NCBI_SRA_normalize, "NCBI:SRA:normalize" },
// #if HAVE_PREFIX_TREE_TO_NAME
//     { NCBI_SRA_prefix_tree_to_name, "NCBI:SRA:prefix_tree_to_name" },
// #endif
//     { NCBI_SRA_qual4_decode, "NCBI:SRA:qual4_decode" },
//     { NCBI_SRA_qual4_decompress_v1, "NCBI:SRA:qual4_decompress_v1" },
// #if HAVE_READ_LEN_FROM_NREADS
//     { NCBI_SRA_read_len_from_nreads, "NCBI:SRA:read_len_from_nreads" },
//     { NCBI_SRA_read_start_from_nreads, "NCBI:SRA:read_start_from_nreads" },
// #endif
//     { NCBI_SRA_read_seg_from_readn, "NCBI:SRA:read_seg_from_readn" },
//     { NCBI_SRA_rewrite_spot_name, "NCBI:SRA:rewrite_spot_name" },
//     { NCBI_SRA_rotate, "NCBI:SRA:rotate" },
//     { NCBI_SRA_swap, "NCBI:SRA:swap" },
//     { NCBI_SRA_syn_quality, "NCBI:SRA:syn_quality" },
//     { NCBI_SRA_syn_quality_read, "NCBI:SRA:syn_quality_read" },

//     { NCBI_WGS_build_read_type, "NCBI:WGS:build_read_type" },
//     { NCBI_WGS_build_scaffold_qual, "NCBI:WGS:build_scaffold_qual" },
//     { NCBI_WGS_build_scaffold_read, "NCBI:WGS:build_scaffold_read" },
//     { NCBI_WGS_tokenize_nuc_accession, "NCBI:WGS:tokenize_nuc_accession" },
//     { NCBI_WGS_tokenize_prot_accession, "NCBI:WGS:tokenize_prot_accession" },

//     { NCBI_align_clip, "NCBI:align:clip" },
//     { NCBI_align_clip_2, "NCBI:align:clip_2" },
//     { NCBI_align_compress_quality, "NCBI:align:compress_quality" },
//     { NCBI_align_decompress_quality, "NCBI:align:decompress_quality" },
//     { NCBI_align_edit_distance, "NCBI:align:edit_distance" },
//     { NCBI_align_edit_distance_2, "NCBI:align:edit_distance_2" },
//     { NCBI_align_edit_distance_3, "NCBI:align:edit_distance_3" },
//     { NCBI_align_generate_mismatch_qual_2, "NCBI:align:generate_mismatch_qual_2" },
//     { NCBI_align_generate_preserve_qual, "NCBI:align:generate_preserve_qual" },
//     { NCBI_align_get_clipped_cigar, "NCBI:align:get_clipped_cigar" },
//     { NCBI_align_get_clipped_cigar_2, "NCBI:align:get_clipped_cigar_2" },
//     { NCBI_align_get_clipped_ref_offset, "NCBI:align:get_clipped_ref_offset" },
//     { NCBI_align_get_left_soft_clip, "NCBI:align:get_left_soft_clip" },
//     { NCBI_align_get_left_soft_clip_2, "NCBI:align:get_left_soft_clip_2" },
//     { NCBI_align_get_mate_align_id, "NCBI:align:get_mate_align_id" },
//     { NCBI_align_get_mismatch_read, "NCBI:align:get_mismatch_read" },
//     { NCBI_align_get_ref_delete, "NCBI:align:get_ref_delete" },
//     { NCBI_align_get_ref_insert, "NCBI:align:get_ref_insert" },
//     { NCBI_align_get_ref_len, "NCBI:align:get_ref_len" },
//     { NCBI_align_get_ref_len_2, "NCBI:align:get_ref_len_2" },
//     { NCBI_align_get_ref_mismatch, "NCBI:align:get_ref_mismatch" },
//     { NCBI_align_get_ref_preserve_qual, "NCBI:align:get_ref_preserve_qual" },
//     { NCBI_align_get_seq_preserve_qual, "NCBI:align:get_seq_preserve_qual" },
//     { NCBI_align_get_right_soft_clip, "NCBI:align:get_right_soft_clip" },
//     { NCBI_align_get_right_soft_clip_2, "NCBI:align:get_right_soft_clip_2" },
//     { NCBI_align_get_right_soft_clip_3, "NCBI:align:get_right_soft_clip_3" },
//     { NCBI_align_get_right_soft_clip_4, "NCBI:align:get_right_soft_clip_4" },
//     { NCBI_align_get_right_soft_clip_5, "NCBI:align:get_right_soft_clip_5" },
//     { NCBI_align_get_sam_flags, "NCBI:align:get_sam_flags" },
//     { NCBI_align_get_sam_flags_2, "NCBI:align:get_sam_flags_2" },
//     { NCBI_align_local_ref_id, "NCBI:align:local_ref_id" },
//     { NCBI_align_local_ref_start, "NCBI:align:local_ref_start" },
//     { NCBI_align_make_cmp_read_desc, "NCBI:align:make_cmp_read_desc" },
//     { NCBI_align_make_read_start, "NCBI:align:make_read_start" },
//     { NCBI_align_mismatch_restore_qual, "NCBI:align:mismatch_restore_qual" },
//     { NCBI_align_not_my_row, "NCBI:align:not_my_row" },
//     { NCBI_align_raw_restore_qual, "NCBI:align:raw_restore_qual" },
//     { NCBI_align_ref_name, "NCBI:align:ref_name" },
//     { NCBI_align_ref_pos, "NCBI:align:ref_pos" },
//     { NCBI_align_ref_seq_id, "NCBI:align:ref_seq_id" },
//     { NCBI_align_ref_sub_select_preserve_qual, "NCBI:align:ref_sub_select_preserve_qual" },
//     { NCBI_align_rna_orientation, "NCBI:align:rna_orientation" },
//     { NCBI_align_seq_construct_read, "NCBI:align:seq_construct_read" },
//     { NCBI_align_template_len, "NCBI:align:template_len" },
//     { NCBI_color_from_dna, "NCBI:color_from_dna" },
//     { NCBI_dna_from_color, "NCBI:dna_from_color" },
//     { NCBI_SRA_useRnaFlag, "NCBI:SRA:useRnaFlag" },
//     { NCBI_fp_extend, "NCBI:fp_extend" },
//     { NCBI_lower_case_tech_reads, "NCBI:lower_case_tech_reads" },
//     { NCBI_unpack, "NCBI:unpack" },
//     { NCBI_unzip, "NCBI:unzip" },
//     { NCBI_var_tokenize_var_id, "NCBI:var:tokenize_var_id" },

    { sra_hello, "sra:hello" }
};

// static VLinkerIntSpecial sra_special [] =
// {
//     { NCBI_SRA_accept_untyped, "NCBI:SRA:accept_untyped" },
//     { NCBI_SRA__454__untyped_0, "NCBI:SRA:_454_:untyped_0" },
//     { NCBI_SRA__454__untyped_1_2a, "NCBI:SRA:_454_:untyped_1_2a" },
//     { NCBI_SRA__454__untyped_1_2b, "NCBI:SRA:_454_:untyped_1_2b" },
//     { NCBI_SRA_Illumina_untyped_0a, "NCBI:SRA:Illumina:untyped_0a" },
//     { NCBI_SRA_Illumina_untyped_0b, "NCBI:SRA:Illumina:untyped_0b" },
//     { NCBI_SRA_Illumina_untyped_1a, "NCBI:SRA:Illumina:untyped_1a" },
//     { NCBI_SRA_Illumina_untyped_1b, "NCBI:SRA:Illumina:untyped_1b" },
//     { NCBI_SRA_ABI_untyped_1, "NCBI:SRA:ABI:untyped_1" }
// };

static
rc_t CC sra_hello_func(
                 void *Self,
                 const VXformInfo *info,
                 int64_t row_id,
                 VRowResult *rslt,
                 uint32_t argc,
                 const VRowData argv[]
)
{
    rc_t rc = KDataBufferResize ( rslt->data, 5 );
    if ( rc == 0 )
    {
        ((char*)rslt->data->base)[0]='h';
        ((char*)rslt->data->base)[1]='e';
        ((char*)rslt->data->base)[2]='l';
        ((char*)rslt->data->base)[3]='l';
        ((char*)rslt->data->base)[4]='o';
        rslt -> elem_count = 5;
    }
    return rc;
}
struct self_t {
    KDataBuffer val;
    bitsz_t csize;
    bitsz_t dsize;
    int count;
};

static void CC self_free( void *Self ) {
    struct self_t *self = (struct self_t *)Self;

    KDataBufferWhack(&self->val);
    free(self);
}

rc_t sra_hello ( const VXfactInfo *info, VFuncDesc *rslt, const VFactoryParams *cp )
{
    rc_t rc;
    struct self_t *self = (struct self_t *) malloc ( sizeof *self );
    if ( self == NULL )
        return RC(rcXF, rcFunction, rcConstructing, rcMemory, rcExhausted);

    self->dsize = VTypedescSizeof ( & cp->argv[0].desc );
    self->csize = self->dsize * cp->argv[0].count;
    self->count = 1;

    rc = KDataBufferMake(&self->val, self->dsize, cp->argv[0].count);
    if (rc == 0) {
        bitcpy(self->val.base, 0, cp->argv[0].data.u8, 0, self->csize);

        rslt->self = self;
        rslt->whack = self_free;
        rslt->variant = vftRow;
        rslt->u.rf = sra_hello_func;
        return 0;
    }
    free(self);
    return rc;
}

VTRANSFACT_IMPL ( sra_hello, 1, 0, 0 ) ( const void *self, const VXfactInfo *info,
    VFuncDesc *rslt, const VFactoryParams *cp, const VFunctionParams *dp )
{
    return sra_hello ( info, rslt, cp );
}

rc_t SraLinkSchema ( const VDBManager * mgr )
{
    return VDBManagerAddFactories ( mgr,  sra_fact, sizeof sra_fact / sizeof sra_fact [ 0 ] );
}
