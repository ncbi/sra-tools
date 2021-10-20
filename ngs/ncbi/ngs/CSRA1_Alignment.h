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

#ifndef _h_csra1_alignment_
#define _h_csra1_alignment_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */

struct VCursor;
struct VDatabase;
struct NGS_String;
struct NGS_Cursor;
struct NGS_Alignment;
struct CSRA1_ReadCollection;

struct NGS_Cursor const* CSRA1_AlignmentMakeDb( ctx_t ctx,
                                                struct VDatabase const* db,
                                                struct NGS_String const* run_name,
                                                char const* table_name );

struct NGS_Alignment * CSRA1_AlignmentMake ( ctx_t ctx, 
                                             struct CSRA1_ReadCollection * coll,
                                             int64_t alignId, 
                                             char const* run_name, size_t run_name_size,
                                             bool primary, 
                                             uint64_t id_offset );

struct NGS_Alignment * CSRA1_AlignmentIteratorMake( ctx_t ctx, 
                                                    struct CSRA1_ReadCollection * coll,
                                                    bool primary, 
                                                    bool secondary, 
                                                    const struct NGS_String  * run_name, 
                                                    uint64_t id_offset );

struct NGS_Alignment * CSRA1_AlignmentRangeMake( ctx_t ctx, 
                                                 struct CSRA1_ReadCollection * coll,
                                                 bool primary, 
                                                 bool secondary, 
                                                 const struct NGS_String  * run_name, 
                                                 uint64_t id_offset,
                                                 int64_t first,
                                                 uint64_t count);

struct NGS_Alignment * CSRA1_AlignmentIteratorMakeEmpty( ctx_t ctx );
                                                 
#ifdef __cplusplus
}
#endif

#endif /* _h_csra1_alignment_ */
