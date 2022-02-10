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
 */

struct BAM_ReadCollection;

typedef struct BAM_Record BAM_Record;
struct BAM_Record {
    uint8_t const *extra;
    char const *QNAME;
    char const *RNAME;
    char const *RNEXT;
    char const *SEQ;
    char const *QUAL;

    uint32_t POS;
    uint32_t PNEXT;

    int32_t TLEN;
    int32_t REFID;

    uint32_t seqlen;
    uint32_t ncigar;
    uint32_t extralen;

    uint16_t FLAG;
    uint8_t MAPQ;
    uint8_t padd;

    uint32_t cigar[1];
};

typedef struct BAM_Record_Extra_Field BAM_Record_Extra_Field;
struct BAM_Record_Extra_Field {
    char const *tag;
    union {
        uint8_t   u8[4]; /* for val_type == 'C' */
        uint16_t u16[2]; /* for val_type == 'S' */
        uint32_t u32[1]; /* for val_type == 'I' */
        int8_t    i8[4]; /* for val_type == 'c' */
        int16_t  i16[2]; /* for val_type == 's' */
        int32_t  i32[1]; /* for val_type == 'i' */
        float    f32[1]; /* for val_type == 'f' */
        char  string[1]; /* for val_type == 'A', 'Z', or 'H'
                          * for 'Z' and 'H' elemcount is strlen 
                          * but the value is still null terminated */
    } const *value;      /* all multi-byte values are little-endian,
                          * byte swap as needed */
    int elemcount;
    int fieldsize;
    char val_type;
};

typedef bool (*BAM_Record_ForEachExtra_cb)( void *usr_ctx, ctx_t ctx,
                                            unsigned ordinal,
                                            BAM_Record_Extra_Field const *fld );

unsigned BAM_Record_ForEachExtra(BAM_Record const *self, ctx_t ctx,
                                 BAM_Record_ForEachExtra_cb, void *usr_ctx);

BAM_Record *BAM_GetRecord(struct NGS_Refcount *const self, ctx_t ctx);
