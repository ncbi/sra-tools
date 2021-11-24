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

#ifndef _h_ngs_id_
#define _h_ngs_id_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_String;

/*--------------------------------------------------------------------------
 * NGS_Id
 *  a unique identifier of an NGS object
 * Represented with a string of the following format:
 *   <runName>.<objectType>.<rowId>
 * where 
 *   rowId is the object's row number in the corresponding table
 *   object type is one of "R" (read), 
 *                         "FR<fragId>" (fragment; rowId refers to the fragment's read, 
 *                         "FA<fragId>" (fragment; rowId refers to the fragment's alignment, 
 *                                          fragId is the fragment's 0-based number within the read; 
 *                                          only biological fragments are counted), 
 *                         "PA" (primary alignment), 
 *                         "SA" (secondary alignment)
 *   runName is the name of the accession
 */

enum NGS_Object
{
    NGSObject_Read,
    NGSObject_ReadFragment,
    NGSObject_AlignmentFragment,
    NGSObject_PrimaryAlignment,
    NGSObject_SecondaryAlignment,
};

struct NGS_Id
{
    String run;
    int64_t rowId;
    int32_t object; /* enum NGS_Object */
    uint32_t fragId;
};

/* Make
 * the returned NGS_String is guaranteed to be NUL-terminated
 */
struct NGS_String* NGS_IdMake ( ctx_t ctx, const struct NGS_String * run, enum NGS_Object object, int64_t rowId );
struct NGS_String* NGS_IdMakeFragment ( ctx_t ctx, const struct NGS_String * run, bool alignment, int64_t rowId, uint32_t frag_num );

struct NGS_Id NGS_IdParse ( char const * self, size_t self_size, ctx_t ctx );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_id */
