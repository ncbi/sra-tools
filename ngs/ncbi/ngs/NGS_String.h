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

#ifndef _h_ngs_string_
#define _h_ngs_string_

typedef struct NGS_String NGS_String;
#ifndef NGS_STRING
#define NGS_STRING NGS_String
#endif

#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_STRING
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * NGS_String
 *  a reference into NGS string data
 *  reference counted, temporary
 */


/* Make
 */
NGS_String * NGS_StringMake ( ctx_t ctx, const char * data, size_t size );
NGS_String * NGS_StringMakeOwned ( ctx_t ctx, char * owned_data, size_t size );
NGS_String * NGS_StringMakeCopy ( ctx_t ctx, const char * temp_data, size_t size );

/* TEMPORARY */
NGS_String * NGS_StringFromI64 ( ctx_t ctx, int64_t i );

/* Invalidate
 */
void NGS_StringInvalidate ( NGS_String * self, ctx_t ctx );


/* Release
 *  release reference
 */
void NGS_StringRelease ( const NGS_String * self, ctx_t ctx );


/* Duplicate
 *  duplicate reference
 */
NGS_String * NGS_StringDuplicate ( const NGS_String * self, ctx_t ctx );


/* Data
 *  retrieve data pointer
 */
const char * NGS_StringData ( const NGS_String * self, ctx_t ctx );


/* Size
 *  retrieve data size
 */
size_t NGS_StringSize ( const NGS_String * self, ctx_t ctx );


/* Substr
 *  create a new allocation
 *  returns a substring, either at a simple offset
 *  or at an offset with size
 */
NGS_String * NGS_StringSubstrOffset ( const NGS_String * self, ctx_t ctx, uint64_t offset );
NGS_String * NGS_StringSubstrOffsetSize ( const NGS_String * self, ctx_t ctx, uint64_t offset, uint64_t size );


/* MakeNULTerminatedString
 *  allocates a NUL-terminated version of self
 *  returned value should be disposed using "free()"
 */
char * NGS_StringMakeNULTerminatedString ( const NGS_String * self, ctx_t ctx );


#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_string_ */
