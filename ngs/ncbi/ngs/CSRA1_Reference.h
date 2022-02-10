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

#ifndef _h_csra1_reference_
#define _h_csra1_reference_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_Cursor;
struct NGS_ReadCollection;
struct NGS_Reference;
struct VDatabase;


enum ReferenceTableColumns
{
    reference_CIRCULAR,
    reference_NAME,
    reference_SEQ_ID,
    reference_SEQ_LEN,
    reference_SEQ_START,
    reference_MAX_SEQ_LEN,
    reference_READ,
    reference_PRIMARY_ALIGNMENT_IDS,
    reference_SECONDARY_ALIGNMENT_IDS,
    reference_OVERLAP_REF_LEN,
    reference_OVERLAP_REF_POS,
    reference_CMP_READ,

    reference_NUM_COLS
};

extern const char * reference_col_specs [];

struct NGS_Reference * CSRA1_ReferenceMake ( ctx_t ctx,
                                             struct NGS_ReadCollection * coll,
                                             const struct VDatabase * db,
                                             const struct NGS_Cursor * curs,
                                             const char * spec,
                                             uint64_t align_id_offset );

struct NGS_Reference * CSRA1_ReferenceIteratorMake ( ctx_t ctx,
                                                     struct NGS_ReadCollection * coll,
                                                     const struct VDatabase * db,
                                                     const struct NGS_Cursor * curs,
                                                     uint64_t align_id_offset );

int64_t CSRA1_Reference_GetFirstRowId ( const struct NGS_Reference * self, ctx_t ctx );
int64_t CSRA1_Reference_GetLastRowId ( const struct NGS_Reference * self, ctx_t ctx );

/*
CSRA1_ReferenceFind
    curs - the cursor opened for table REFERENCE
    spec - the reference name to find
    firstRow [OUT, NULL OK] - the first row id for the reference found in the case when the function returns non-zero
    rowCount  [OUT, NULL OK] - the number of row ids for the reference found in the case when the function returns non-zero

    return: 0 (false) if the reference is not found, otherwise (true) - the reference is found
*/
bool CSRA1_ReferenceFind ( struct NGS_Cursor const * curs, ctx_t ctx, const char * spec, int64_t* firstRow, uint64_t* rowCount );

#ifdef __cplusplus
}
#endif

#endif /* _h_csra1_reference_ */
