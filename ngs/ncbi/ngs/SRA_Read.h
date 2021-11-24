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

#ifndef _h_sra_read_
#define _h_sra_read_

typedef struct SRA_Read SRA_Read;

#ifndef NGS_READ
#define NGS_READ SRA_Read
#endif

#ifndef _h_ngs_read_
#include "NGS_Read.h"
#endif

#ifndef _h_insdc_insdc_
#include <insdc/insdc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_Cursor;
struct NGS_String;

enum SequenceTableColumn
{   /* keep in sync with sequence_col_specs in SRA_Read.c */
    seq_READ,
    seq_READ_TYPE,
    seq_QUALITY,
    seq_READ_LEN,
    seq_NAME,
    seq_GROUP,
    seq_PRIMARY_ALIGNMENT_ID,
    seq_SPOT_COUNT,
    seq_CMP_READ,

    seq_NUM_COLS
};  /* keep in sync with sequence_col_specs in SRA_Read.c */

extern const char * sequence_col_specs [];

/*--------------------------------------------------------------------------
 * SRA_Read
 */
struct SRA_Read
{
    NGS_Read dad;

    struct NGS_String * run_name;
    struct NGS_String * group_name; /* if not NULL, only return reads from this read group */

    int64_t cur_row;
    int64_t row_max;
    uint64_t row_count;

    const INSDC_read_type * READ_TYPE;
    const INSDC_coord_len * READ_LEN;

    struct NGS_Cursor const * curs;

    uint32_t cur_frag;
    uint32_t bio_frags;
    uint32_t frag_idx;
    uint32_t frag_max;
    uint32_t frag_start;
    uint32_t frag_len;

    bool seen_first;
    bool seen_first_frag;
    bool seen_last_frag;

    /* read filtering criteria */
    bool wants_full;
    bool wants_partial;
    bool wants_unaligned;
};

void                     SRA_ReadWhack ( NGS_READ * self, ctx_t ctx );

struct NGS_String * SRA_FragmentGetId ( NGS_READ * self, ctx_t ctx );
struct NGS_String * SRA_FragmentGetSequence ( NGS_READ * self, ctx_t ctx, uint64_t offset, uint64_t length );
struct NGS_String * SRA_FragmentGetQualities ( NGS_READ * self, ctx_t ctx, uint64_t offset, uint64_t length );
bool                SRA_FragmentIsPaired ( NGS_READ * self, ctx_t ctx );
bool                SRA_FragmentIsAligned ( NGS_READ * self, ctx_t ctx );
bool                SRA_FragmentNext ( NGS_READ * self, ctx_t ctx );

struct NGS_String *      SRA_ReadGetId ( NGS_READ * self, ctx_t ctx );
struct NGS_String *      SRA_ReadGetName ( NGS_READ * self, ctx_t ctx );
struct NGS_String *      SRA_ReadGetReadGroup ( NGS_READ * self, ctx_t ctx );
enum NGS_ReadCategory    SRA_ReadGetCategory ( const NGS_READ * self, ctx_t ctx );
struct NGS_String *      SRA_ReadGetSequence ( NGS_READ * self, ctx_t ctx, uint64_t offset, uint64_t length );
struct NGS_String *      SRA_ReadGetQualities ( NGS_READ * self, ctx_t ctx, uint64_t offset, uint64_t length );
uint32_t                 SRA_ReadNumFragments ( NGS_READ * self, ctx_t ctx );
bool                     SRA_ReadFragIsAligned ( NGS_READ * self, ctx_t ctx, uint32_t frag_idx );
bool                     SRA_ReadIteratorNext ( NGS_READ * self, ctx_t ctx );
uint64_t                 SRA_ReadIteratorGetCount ( const NGS_READ * self, ctx_t ctx );

/* Make
 * a single read
 */
struct NGS_Read * SRA_ReadMake ( ctx_t ctx, const struct NGS_Cursor * curs, int64_t readId, const struct NGS_String * spec );

/* IteratorMake
 */
struct NGS_Read * SRA_ReadIteratorMake ( ctx_t ctx,
                                               const struct NGS_Cursor * curs,
                                               const struct NGS_String * run_name,
                                               bool wants_full,
                                               bool wants_partial,
                                               bool wants_unaligned );

/* IteratorInitFragment
 */
void SRA_ReadIteratorInitFragment ( NGS_READ * self, ctx_t ctx );

/* IteratorMakeRange
 * all reads in the specified range of rowIds
 */
struct NGS_Read * SRA_ReadIteratorMakeRange ( ctx_t ctx,
                                              const struct NGS_Cursor * curs,
                                              const struct NGS_String * run_name,
                                              uint64_t first,
                                              uint64_t count,
                                              bool wants_full,
                                              bool wants_partial,
                                              bool wants_unaligned );

/* IteratorMakeReadGroup
 * within the specified range of rowIds, will only return reads belonging to the specified read group (groupName)
 */
struct NGS_Read * SRA_ReadIteratorMakeReadGroup ( ctx_t ctx,
                                                  const struct NGS_Cursor * curs,
                                                  const struct NGS_String * run_name,
                                                  const struct NGS_String * group_name,
                                                  uint64_t first,
                                                  uint64_t count,
                                                  bool wants_full,
                                                  bool wants_partial,
                                                  bool wants_unaligned );

#ifdef __cplusplus
}
#endif

#endif /* _h_sra_read_ */
