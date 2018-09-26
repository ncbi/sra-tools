/* ===========================================================================
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

struct bam_alignment_s {
    uint8_t rID[4];
    uint8_t pos[4];
    uint8_t read_name_len;
    uint8_t mapQual;
    uint8_t bin[2];
    uint8_t n_cigars[2];
    uint8_t flags[2];
    uint8_t read_len[4];
    uint8_t mate_rID[4];
    uint8_t mate_pos[4];
    uint8_t ins_size[4];
    char read_name[1 /* read_name_len */];
/* if you change length of read_name,
 * adjust calculation of offsets in BAM_AlignmentSetOffsets */
/*  uint32_t cigar[n_cigars];
 *  uint8_t seq[(read_len + 1) / 2];
 *  uint8_t qual[read_len];
 *  uint8_t extra[...];
 */
};

typedef union bam_alignment_u {
    struct bam_alignment_s cooked;
    uint8_t raw[sizeof(struct bam_alignment_s)];
} bam_alignment;

struct offset_size_s {
    unsigned offset;
    unsigned size; /* this is the total length of the tag; length of data is size - 3 */
};

struct BAM_Alignment {
    struct BAM_File *parent;
    bam_alignment const *data;
    uint8_t *storage;

	uint64_t keyId;
	bool wasInserted;
    bool isRNA;

    unsigned datasize;
    unsigned cigar;
    unsigned seq;
    unsigned qual;
    unsigned numExtra;
    unsigned hasColor;
    struct offset_size_s extra[1];
};
