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

#ifndef _h_search_smith_waterman_
#define _h_search_smith_waterman_

#ifndef _h_search_extern_
#include <search/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SmithWaterman SmithWaterman;

typedef struct SmithWatermanMatch SmithWatermanMatch;
struct SmithWatermanMatch
{
    int32_t position;
    int32_t length;
    int32_t score;
};

/* Make
 */
SEARCH_EXTERN rc_t CC SmithWatermanMake ( SmithWaterman **self, const char *query );

/* Whack
 */
SEARCH_EXTERN void CC SmithWatermanWhack ( SmithWaterman *self );

/* FindFirst
 *  threshold - minimum matching score: 0 will match anything, 2*strlen(query) and higher will only report perfect match
 *  Returns:    0 a match is found (details in the matchinfo, NULL is OK), 
 *              RC(rcText, rcString, rcSearching, rcQuery, rcNotFound) if nothing found, 
 *              other RC in case of an error.
 */
SEARCH_EXTERN rc_t CC SmithWatermanFindFirst ( SmithWaterman *self, uint32_t threshold, const char *buf, size_t buf_size, SmithWatermanMatch* matchinfo );

#ifdef __cplusplus
}
#endif

#endif 
