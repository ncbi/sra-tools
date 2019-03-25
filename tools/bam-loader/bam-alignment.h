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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define HAVE_C99_FAM 1
#ifdef _MSC_VER /* && _MSC_VER < ??? newer version support this language feature */
#define HAVE_C99_FAM 0
#endif

/* this struct is packed and little-endian
 * iow, multi-byte members are little-endian and not aligned
 */
struct bam_alignment_s {
    uint8_t rID[4];         /* int32_t */
    uint8_t pos[4];         /* int32_t */
    uint8_t read_name_len;
    uint8_t mapQual;
    uint8_t bin[2];         /* uint16_t */
    uint8_t n_cigars[2];    /* uint16_t */
    uint8_t flags[2];       /* uint16_t */
    uint8_t read_len[4];    /* int32_t */
    uint8_t mate_rID[4];    /* int32_t */
    uint8_t mate_pos[4];    /* int32_t */
    uint8_t ins_size[4];    /* int32_t */
/*  uint8_t read_name[read_name_len];   // nul-terminated; length includes nul
 *  uint8_t cigar[4 * n_cigars];        // uint32_t
 *  uint8_t seq[(read_len + 1) / 2];    // packed binary 4na
 *  uint8_t qual[read_len];
 *  uint8_t extra[];
 */
#if HAVE_C99_FAM
    char read_name[];
#else
    /* the standard says it is undefined behavior to access beyond the declared
     * end of a struct, but older Microsoft compilers don't support the proper
     * syntax, hence this hack
     */
    char read_name[1];
#endif
};

#define BAM_ALIGNMENT_MIN_SIZE offsetof(struct bam_alignment_s, read_name[0])
typedef union bam_alignment_u {
    struct bam_alignment_s cooked;
    uint8_t raw[BAM_ALIGNMENT_MIN_SIZE]; /* FAM is for structs */
} bam_alignment;

static inline uint16_t bam_alignment_get_u16(uint8_t const *const value) {
    return ((uint16_t)value[0]) | (((uint16_t)value[1]) << 8);
}

static inline int16_t bam_alignment_get_i16(uint8_t const *const value) {
    union { uint16_t u; int16_t i; } y;
    y.u = bam_alignment_get_u16(value);
    return y.i;
}

static inline uint32_t bam_alignment_get_u32(uint8_t const *const value) {
    return ((uint32_t)bam_alignment_get_u16(value))
        | (((uint32_t)bam_alignment_get_u16(value + 2)) << 16);
}

static inline int32_t bam_alignment_get_i32(uint8_t const *const value) {
    union { uint32_t u; int32_t i; } y;
    y.u = bam_alignment_get_u32(value);
    return y.i;
}

static inline int32_t bam_alignment_rID(bam_alignment const *const s) {
    return bam_alignment_get_i32(s->cooked.rID);
}

static inline int32_t bam_alignment_pos(bam_alignment const *const s) {
    return bam_alignment_get_i32(s->cooked.pos);
}

static inline size_t bam_alignment_cigars(bam_alignment const *const s) {
    return bam_alignment_get_u16(s->cooked.n_cigars);
}

static inline size_t bam_alignment_read_len(bam_alignment const *const s) {
    return bam_alignment_get_i32(s->cooked.read_len);
}

static inline size_t bam_alignment_offsetof_cigars(bam_alignment const *const s) {
    return BAM_ALIGNMENT_MIN_SIZE + (size_t)s->cooked.read_name_len;
}

static inline size_t bam_alignment_offsetof_sequence(bam_alignment const *const s) {
    return bam_alignment_offsetof_cigars(s) + 4 * bam_alignment_cigars(s);
}

static inline size_t bam_alignment_offsetof_quality(bam_alignment const *const s) {
    return bam_alignment_offsetof_sequence(s) + (bam_alignment_read_len(s) + 1) / 2;
}

static inline size_t bam_alignment_offsetof_extra(bam_alignment const *const s) {
    return bam_alignment_offsetof_quality(s) + bam_alignment_read_len(s);
}

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

    unsigned datasize;
    unsigned cigar;
    unsigned seq;
    unsigned qual;
    unsigned numExtra;
    unsigned hasColor;
#if HAVE_C99_FAM
    struct offset_size_s extra[];
#else
    struct offset_size_s extra[1];
#endif
};

#define BAM_ALIGNMENT_SIZE(NUM_EXTRA) offsetof(struct BAM_Alignment, extra[NUM_EXTRA])
#define BAM_ALIGNMENT_MAX_EXTRA(BYTES) ((BYTES - BAM_ALIGNMENT_SIZE(0)) / sizeof(struct offset_size_s))
