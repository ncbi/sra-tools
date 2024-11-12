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

#ifndef _h_search_grep_
#define _h_search_grep_

#ifndef _h_search_extern_
#include <search/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifndef _h_insdc_insdc_
#include <insdc/insdc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * Fgrep
 */
typedef struct Fgrep Fgrep;


/* MatchCallback
 *
 *  "matchinfo" [ IN ] -
 *
 *  "flag" [ OUT ] -
 */
typedef uint8_t FgrepContinueFlag;
enum
{
    FGREP_CONTINUE = 1,
    FGREP_STOP = 2
};

typedef struct FgrepMatch FgrepMatch;
struct FgrepMatch
{
    int32_t position;
    int32_t length;
    int32_t whichpattern;
};

typedef rc_t ( CC * FgrepMatchCallback ) ( void *cbinfo,
    const FgrepMatch *matchinfo, FgrepContinueFlag *flag );


/* Make
 *
 *  "fg" [ OUT ] -
 *
 *  "mode" [ IN ]
 *
 *  "strings" [ IN ] and "numstrings" [ IN ] -
 */
typedef uint8_t FgrepFlags;
enum
{
    FGREP_MODE_ASCII = 1,
    FGREP_MODE_ACGT = 2,
    FGREP_TEXT_EXPANDED_2NA = 4,
    FGREP_ALG_DUMB = 8,
    FGREP_ALG_BOYERMOORE = 0x10,
    FGREP_ALG_AHOCORASICK = 0x20
};

SEARCH_EXTERN rc_t CC FgrepMake ( Fgrep **fg, FgrepFlags mode,
    const char *strings[], uint32_t numstrings );

/* Whack
 */
 #if 0
SEARCH_EXTERN void CC FgrepWhack ( Fgrep *self );
#endif
SEARCH_EXTERN void CC FgrepFree ( Fgrep *self );

/* FindFirst
 *  Pass a matchinfo structure to get the info.
 *  Returns nonzero if found, 0 if nothing found.
 *
 *   "buf" [ IN ] and "len" [ IN ]
 *
 *  "matchinfo" [ OUT ]
 */
SEARCH_EXTERN uint32_t CC FgrepFindFirst ( const Fgrep *self,
    const char *buf, size_t len, FgrepMatch *matchinfo );


/* FindAll
 * TBD - should this return rc_t?
 */
 #if 0
SEARCH_EXTERN void CC FgrepFindAll ( const Fgrep *self, const char *buf, size_t len,
    FgrepMatchCallback cb, void *cbinfo );
#endif

/*----------------------------------------------------------------
 * Fgrep appendix 
 */

/*
 * Sees if a suffix of the pattern exact matches a prefix of the buffer.
 */
SEARCH_EXTERN uint32_t CC has_left_exact_match(char *pattern, char *buf, size_t buflen,
                              int32_t *length);

SEARCH_EXTERN uint32_t CC has_right_exact_match(char *pattern, char *buf, size_t buflen, 
                               int32_t *bestpos);

SEARCH_EXTERN uint32_t CC has_inside_exact_match(char *pattern, char *buf, size_t buflen, 
                                int32_t *skip);

/*--------------------------------------------------------------------------
 * Agrep
 */
typedef struct Agrep Agrep;


/* MatchCallback
 */
typedef struct AgrepMatch AgrepMatch;
struct AgrepMatch
{
    int32_t position;
    int32_t length;
    int32_t score;
};

typedef uint8_t AgrepContinueFlag;
enum
{
    AGREP_STOP = 1,
    AGREP_CONTINUE = 2,
    AGREP_MOREINFO = 4
};

typedef rc_t ( CC * AgrepMatchCallback ) ( const void *cbinfo, const AgrepMatch *matchinfo, AgrepContinueFlag *flag );


/* Make
 */
typedef uint32_t AgrepFlags;
enum
{
    AGREP_MODE_ASCII = 0x0001, /* simple text grep */
    AGREP_PATTERN_4NA = 0x0002, /* pattern is 4na - bio mode */
    /* text flags */
    AGREP_IGNORE_CASE = 0x0004, /* ignore case in ascii mode, bio mode always ignores case */
    /* bio mode flags */
    AGREP_TEXT_EXPANDED_2NA = 0x0008, /* search string can have 2na */
    AGREP_ANYTHING_ELSE_IS_N = 0x0010, /* periods, dashes in 4na pattern treated as N */
    
    /* algorithm choice */
    AGREP_ALG_DP = 0x0020,
    AGREP_ALG_WUMANBER = 0x0040,
    AGREP_ALG_MYERS = 0x0080, /* shoud be same, internally detect algo based on pattern size */
    AGREP_ALG_MYERS_UNLTD = 0x0100, /* very faulty, at least will crash on low memory */

    AGREP_EXTEND_SAME = 0x200,
    AGREP_EXTEND_BETTER = 0x400,
    AGREP_LEFT_MAINTAIN_SCORE = 0x800, /* Only supported in DP for now */
    AGREP_ANCHOR_LEFT = 0x1000 /* Only supported in DP for now */
};

SEARCH_EXTERN rc_t CC AgrepMake(Agrep **self, AgrepFlags mode, const char *pattern);

/* Whack
 */
SEARCH_EXTERN void CC AgrepWhack ( Agrep *self );

/* FindFirst
 *  Pass in a pointer to an AgrepMatch, and it will be filled out.
 *  Returns nonzero if something found, zero if nothing found.
 */
SEARCH_EXTERN uint32_t CC AgrepFindFirst( const Agrep *self, int32_t threshold, const char *buf, size_t len, AgrepMatch *matchinfo);

SEARCH_EXTERN uint32_t CC AgrepFindBest( const Agrep *self, int32_t threshold, const char *buf, int32_t len, AgrepMatch *match);

/* FindAll
 */
typedef struct AgrepCallArgs AgrepCallArgs;
struct AgrepCallArgs
{
    const Agrep *self;

    const char *buf;
    size_t buflen;

    AgrepMatchCallback cb;
    void *cbinfo;

    /*
      This threshold is really intended for algorithms like DP and MYERS
      that do not require the threshold to be part of the initial setup.
      We may use this to override the one specified in the setup,
      but we may not; if you want to use the one in the setup function,
      pass -1 as the threshold.
    */
    int32_t threshold;
};

SEARCH_EXTERN void CC AgrepFindAll ( const AgrepCallArgs *args );

/*--------------------------------------------------------------------------
 * Agrep appendix
 */

SEARCH_EXTERN uint32_t CC has_left_approx_match(char *pattern, uint32_t errors, 
                               char *buf, size_t buflen, 
                               uint32_t *length, uint32_t *errcnt);

SEARCH_EXTERN uint32_t CC has_right_approx_match(char *pattern, uint32_t errors, 
                                char *buf, size_t buflen, 
                                uint32_t *bestpos, uint32_t *errcnt);

SEARCH_EXTERN uint32_t CC has_inside_approx_match(char *pattern, uint32_t plen, uint32_t errors, 
                                 char *buf, size_t buflen, 
                                 uint32_t *skip, uint32_t *errcnt);

typedef struct LeftMatch LeftMatch;
struct LeftMatch
{
    int32_t position;
    int32_t hits;
    int32_t misses;
};

typedef rc_t ( CC * LeftMatchCallback ) ( void *cbinfo, const LeftMatch *matchinfo, AgrepContinueFlag *flag );

SEARCH_EXTERN size_t CC FindLongestCommonSubstring (
    char const* pS1, char const* pS2, size_t const nLen1, size_t const nLen2,
    size_t* pRetStart1, size_t* pRetStart2);

#ifdef __cplusplus
}
#endif

#endif /* _h_search_grep_ */
