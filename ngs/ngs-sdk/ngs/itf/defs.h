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

#ifndef _h_ngs_itf_defs_
#define _h_ngs_itf_defs_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * calling convention
 */
#ifndef CC
 #if defined _MSC_VER
  #define CC __cdecl
 #else
  #define CC
 #endif
#endif

/*--------------------------------------------------------------------------
 * NGS_ErrBlock
 *  see "ErrBlock.h"
 */
typedef struct NGS_ErrBlock_v1 NGS_ErrBlock_v1;

/*--------------------------------------------------------------------------
 * NGS_String
 *  see "StringItf.h"
 */
typedef struct NGS_String_v1 NGS_String_v1;

/*--------------------------------------------------------------------------
 * Dynamic exception specification throw(...), deprecated in C++11
 */
#if defined (__cplusplus)
    #if __cplusplus >= 201103L
        #define NGS_THROWS(...)
        #define NGS_NOTHROW() noexcept
    #else
        #define NGS_THROWS(...) throw(__VA_ARGS__)
        #define NGS_NOTHROW() throw()
    #endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_itf_defs_ */
