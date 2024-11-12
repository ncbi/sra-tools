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

#include <search/extern.h>
#include <compiler.h>
#include <os-native.h>
#include <sysalloc.h>
#include <assert.h>
#include "search-priv.h"
#include "debug.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define DBG_SEARCH_METHOD DBG_SEARCH_MYERS

#define UBITTYPE uint64_t

struct MyersSearch {
    AgrepFlags mode;
    int32_t m;
    UBITTYPE PEq[256];
    UBITTYPE PEq_R[256];
};

static
rc_t myers_translate(AgrepFlags mode, UBITTYPE* PEq, unsigned char p, UBITTYPE val)
{
    /* For now always set 2na bits */
    set_bits_2na(PEq, p, val);

    if( mode & AGREP_PATTERN_4NA ) {
        return na4_set_bits(mode, PEq, p, val);
    } else if( mode & AGREP_MODE_ASCII ) {
        if( mode & AGREP_IGNORE_CASE ) {
            PEq[tolower(p)] |= val;
            PEq[toupper(p)] |= val;
        } else {
            PEq[p] |= val;
        }
    }
    return 0;
}

#if _DEBUGGING
static
void printbits( UBITTYPE in )
{
    char buf[9];
    int32_t j, k;
    unsigned char byte;
    buf[8] = '\0';
    for (j=0; j<sizeof(UBITTYPE); j++) {
        byte = in >> (8*(sizeof(UBITTYPE) - j - 1));
        for (k=0; k<8; k++) {
            buf[k] = "01"[1&(byte>>(7-k))];
        }
        SEARCH_DBGF(("%s ", buf));
    }
}
#else
#define printbits(in) ((void)0)
#endif

void AgrepMyersFree( MyersSearch *self )
{
    free(self);
}
  
rc_t AgrepMyersMake( MyersSearch **self, AgrepFlags mode, const char *pattern )
{
    rc_t rc = 0;
    uint32_t max_pattern_length = sizeof(UBITTYPE) * 8;
    int32_t m = strlen(pattern);

    *self = NULL;
    if( m > max_pattern_length ) {
        rc = RC(rcText, rcString, rcSearching, rcParam, rcExcessive);
    } else if( (*self = malloc(sizeof(**self))) == NULL ) {
        rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
    } else {            
        int32_t j;
        const unsigned char *upattern = (const unsigned char *)pattern;

        (*self)->m = m;
        (*self)->mode = mode;
        memset((*self)->PEq, 0, sizeof((*self)->PEq));
        for(j = 0; rc == 0 && j < m; j++) {
            rc = myers_translate(mode, (*self)->PEq, upattern[j], (UBITTYPE)1 << j);
        }
        SEARCH_DBG("pattern '%s'", upattern);
        for(j = 0; j < sizeof((*self)->PEq) / sizeof((*self)->PEq[0]); j++) {
            if( (*self)->PEq[j] ) {
                printbits((*self)->PEq[j]);
                SEARCH_DBGF((" <- %c\n", j));
            }
        }
        memset((*self)->PEq_R, 0, sizeof((*self)->PEq_R));
        for(j = 0; rc == 0 && j < m; j++) {
            rc = myers_translate(mode, (*self)->PEq_R, upattern[m - j - 1], (UBITTYPE)1 << j);
        }
        SEARCH_DBG("pattern rev '%s'", upattern);
        for(j = 0; j < sizeof((*self)->PEq) / sizeof((*self)->PEq[0]); j++) {
            if( (*self)->PEq[j] ) {
                printbits((*self)->PEq[j]);
                SEARCH_DBGF((" <- %c\n", j));
            }
        }
    }
    return rc;
}

/* 
   This finds the first match forward in the string less than or equal
   the threshold.  If nonzero, this will be a prefix of any exact match.
*/

uint32_t MyersFindFirst( MyersSearch *self, int32_t threshold, 
                   const char* text, size_t n,
                   AgrepMatch *match )
{
    const unsigned char *utext = (const unsigned char *)text;
    UBITTYPE Pv;
    UBITTYPE Mv;

    int32_t m = self->m;
    int32_t Score;
    int32_t BestScore = m;
    int32_t from = 0;
    int32_t to = -1;

    int32_t j;
    UBITTYPE Eq, Xv, Xh, Ph, Mh;

    Score = m;
    Pv = (UBITTYPE)-1;
    Mv = (UBITTYPE)0;
    
    for(j = 0; j < n; j++) {
        Eq = self->PEq[utext[j]];
        Xv = Eq | Mv;
        Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
        Ph = Mv | ~ (Xh | Pv);
        Mh = Pv & Xh;
        if(Ph & ((UBITTYPE)1 << (m - 1))) {
            Score++;
        } else if(Mh & ((UBITTYPE)1 << (m - 1))) {
            Score--;
        }
        Ph <<= 1;
        Mh <<= 1;
        Pv = Mh | ~(Xv | Ph);
        Mv = Ph & Xv;
        SEARCH_DBG("1st: %3d. '%c' score %d", j, utext[j], Score);
        if (Score <= threshold) {
            BestScore = Score;
            to = j;
            break;
        }
    }

    if (BestScore <= threshold) {

        /* Continue while score decreases under the threshold */
        for(j++; j < n; j++) {
            Eq = self->PEq[utext[j]];
            Xv = Eq | Mv;
            Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
            Ph = Mv | ~ (Xh | Pv);
            Mh = Pv & Xh;
            if(Ph & ((UBITTYPE)1 << (m - 1))) {
                Score++;
            } else if(Mh & ((UBITTYPE)1 << (m - 1))) {
                Score--;
            }
            Ph <<= 1;
            Mh <<= 1;
            Pv = Mh | ~(Xv | Ph);
            Mv = Ph & Xv;
            SEARCH_DBG("2nd: %3d. '%c' score %d", j, utext[j], Score);
            if( Score < BestScore ||
                ( (self->mode & (AGREP_EXTEND_BETTER | AGREP_EXTEND_SAME)) && Score <= BestScore) ) {
                BestScore = Score;
                to = j;
            } else {
                break;
            }
        }
    }

    if (BestScore <= threshold) {
        /* Re-initialize for next scan! */
        Score = m;
        Pv = (UBITTYPE)-1;
        Mv = (UBITTYPE)0;
        
        for(j = to; j >= 0; j--) {
            Eq = self->PEq_R[utext[j]];
            Xv = Eq | Mv;
            Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
            Ph = Mv | ~ (Xh | Pv);
            Mh = Pv & Xh;
            if(Ph & ((UBITTYPE)1 << (m - 1))) {
                Score++;
            } else if(Mh & ((UBITTYPE)1 << (m - 1))) {
                Score--;
            }
            Ph <<= 1;
            Mh <<= 1;
            Pv = Mh | ~(Xv | Ph);
            Mv = Ph & Xv;
            SEARCH_DBG("Rvs: %3d. '%c' score %d", j, utext[j], Score);
            if(Score <= BestScore) {
                from = j;
                break;
            }
        }
    }
    if (BestScore <= threshold) {
        match->position = from;
        match->length = to-from+1;
        match->score = BestScore;
        SEARCH_DBG("Hit: [%d,%d] '%.*s' score %d", match->position, match->length,
                    match->length, &utext[match->position], match->score);
        return 1;
    }
    return 0;
}

  

/* 
   Returns non-negative if something found.
   Return value is the number of mismatches.
*/
LIB_EXPORT int32_t CC MyersFindBest ( MyersSearch *self, const char* text, 
    size_t n, int32_t *pos, int32_t *len )
{
    const unsigned char *utext = (const unsigned char *)text;
    UBITTYPE Pv;
    UBITTYPE Mv;

    int32_t m = self->m;
    int32_t Score;
    int32_t BestScore = m;
    int32_t from = 0;
    int32_t to = -1;

    int32_t j;
    UBITTYPE Eq, Xv, Xh, Ph, Mh;

    Score = m;
    Pv = (UBITTYPE)-1;
    Mv = (UBITTYPE)0;
    
    for(j = 0; j < n; j++) {
        Eq = self->PEq[utext[j]];
        Xv = Eq | Mv;
        Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
        Ph = Mv | ~ (Xh | Pv);
        Mh = Pv & Xh;
        if(Ph & ((UBITTYPE)1 << (m - 1))) {
            Score++;
        } else if(Mh & ((UBITTYPE)1 << (m - 1))) {
            Score--;
        }
        Ph <<= 1;
        Mh <<= 1;
        Pv = Mh | ~(Xv | Ph);
        Mv = Ph & Xv;
        SEARCH_DBG("Fwd: %3d. '%c' score %d", j, utext[j], Score);
        if( Score < BestScore ) {
            BestScore = Score;
            to = j;
        }
    }

    /* Re-initialize for next scan! */
    Score = m;
    Pv = (UBITTYPE)-1;
    Mv = (UBITTYPE)0;
    
    for(j = to; j >= 0; j--) {
        Eq = self->PEq_R[utext[j]];
        Xv = Eq | Mv;
        Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
        Ph = Mv | ~ (Xh | Pv);
        Mh = Pv & Xh;
        if(Ph & ((UBITTYPE)1 << (m - 1))) {
            Score++;
        } else if(Mh & ((UBITTYPE)1 << (m - 1))) {
            Score--;
        }
        Ph <<= 1;
        Mh <<= 1;
        Pv = Mh | ~(Xv | Ph);
        Mv = Ph & Xv;
        SEARCH_DBG("Rvs: %3d. '%c' score %d", j, utext[j], Score);
        if(Score <= BestScore) {
            from = j;
            break;
        }
    }
    
    *pos = from;
    *len = to-from+1;
    SEARCH_DBG("Hit: [%d,%d] '%.*s' score %d", *pos, *len, *len, &utext[*pos], BestScore);
    return BestScore;
}

/* 
   This finds the first match forward in the string less than or equal
   the threshold.  If nonzero, this will be a prefix of any exact match.
*/

/*
    MyersFindAllOld is the old implementation of MyersFindAll.
    It uses inline copy-pasted Myers core algorithm steps therefore
    it might be faster than the new MyersFindAll (if compiler doesn't optimize it enough)
    Also for some reason it uses unoptimized dp algorithm to find starting point of the match.

    Leaving it in CVS for a while just in case (with fixed bug in "const void *cbinfo = args->cbinfo;"
    and some minor improvements like marking data being used for read only as explicitly const)
*/
#if 0
void MyersFindAllOld ( const AgrepCallArgs *args )
{
    AgrepFlags const mode = args->self->mode;
    MyersSearch const* self = args->self->myers;
    int32_t const threshold = args->threshold;
    const unsigned char *utext = (const unsigned char *)args->buf;
    int32_t const n = args->buflen;
    AgrepMatchCallback cb = dp_end_callback;
    const void *cbinfo = args->cbinfo;

    AgrepMatch match;
    AgrepContinueFlag cont;

    UBITTYPE Pv;
    UBITTYPE Mv;

    int32_t const m = self->m;
    int32_t Score;
    int32_t BestScore;

    int32_t j;
    UBITTYPE Eq, Xv, Xh, Ph, Mh;

    int32_t curscore = 0;
    int32_t curlast = 0;
    int32_t continuing = 0;
    
    BestScore = m;
    Score = m;
    Pv = (UBITTYPE)-1;
    Mv = (UBITTYPE)0;
    
    for (j = 0; j < n; j++)
    {
        Eq = self->PEq[utext[j]];
        Xv = Eq | Mv;
        Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
        Ph = Mv | ~ (Xh | Pv);
        Mh = Pv & Xh;

        SEARCH_DBG("%d: Ph ", j);
        printbits(Ph);
        SEARCH_DBG(" %s ", "Mh");
        printbits(Mh);
        SEARCH_DBG("%s", "");

        if(Ph & ((UBITTYPE)1 << (m - 1))) {
            Score++;
        } else if(Mh & ((UBITTYPE)1 << (m - 1))) {
            Score--;
        }
        Ph <<= 1;
        Mh <<= 1;
        Pv = Mh | ~(Xv | Ph);
        Mv = Ph & Xv;
        SEARCH_DBG("%3d. '%c' score %d", j, utext[j], Score);
        if (Score <= threshold) {
            /* At this point we let the DP algorithm find the starting point. */
            /* This just passes the end position. */

            if (continuing) {
                if (Score < curscore &&
                    ((mode & AGREP_EXTEND_BETTER) ||
                     (mode & AGREP_EXTEND_SAME))) {
                    curscore = Score;
                    curlast = j;
                } else if (Score == curscore &&
                           ((mode & AGREP_EXTEND_BETTER) ||
                            (mode & AGREP_EXTEND_SAME))) {
                    if (mode & AGREP_EXTEND_SAME) {
                        curlast = j;
                    }
                } else {
                    continuing = 0;
                    match.score = curscore;
                    match.position = curlast;
                    match.length = -1;
                    cont = AGREP_CONTINUE;
                    (*cb)(cbinfo, &match, &cont);
                    if (cont != AGREP_CONTINUE)
                        return;
                }
            } else if ((mode & AGREP_EXTEND_SAME) ||
                       (mode & AGREP_EXTEND_BETTER)) {
                curscore = Score;
                curlast = j;
                continuing = 1;
            } else {
                match.score = Score;
                match.position = j;
                match.length = -1;
                cont = AGREP_CONTINUE;
                (*cb)(cbinfo, &match, &cont);
                if (cont != AGREP_CONTINUE)
                    return;
            }
            /* If we're no longer under the threshold, we might
               have been moving forward looking for a better match 
            */
        } else if (continuing) {
            continuing = 0;
            match.score = curscore;
            match.position = curlast;
            match.length = -1;
            cont = AGREP_CONTINUE;
            (*cb)(cbinfo, &match, &cont);
            if (cont != AGREP_CONTINUE)
                return;
        }
    }
    if (continuing) {
        continuing = 0;
        match.score = curscore;
        match.position = curlast;
        match.length = -1;
        (*cb)(cbinfo, &match, &cont);
    }
}
#endif


/* The core of the Myers algorithm - calculation of j-th score
    Now using for new MyersFindAll only but can be used by all other Myers-find
    functons if there is no issues with performance because of function call with
    so many parameters and locals
*/

#define MACRO_MYERS_CORE_STEP(ch, m, PEq, Mv, Pv, Score)\
    Eq = PEq[ch];\
    Xv = Eq | Mv;\
    Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;\
    Ph = Mv | ~ (Xh | Pv);\
    Mh = Pv & Xh;\
    if (Ph & ((UBITTYPE)1 << (m - 1)))\
        ++(Score);\
    else if (Mh & ((UBITTYPE)1 << (m - 1)))\
        --(Score);\
\
    Ph <<= 1;\
    Mh <<= 1;\
    Pv = Mh | ~(Xv | Ph);\
    Mv = Ph & Xv;


static void MyersCoreStep(unsigned char ch, int32_t const m
    ,UBITTYPE const* PEq, UBITTYPE* Mv, UBITTYPE* Pv, int32_t *Score
)
{
    UBITTYPE Eq, Xv, Xh, Ph, Mh;

    Eq = PEq[ch];
    Xv = Eq | *Mv;
    Xh = (((Eq & *Pv) + *Pv) ^ *Pv) | Eq;
    Ph = *Mv | ~ (Xh | *Pv);
    Mh = *Pv & Xh;
    if (Ph & ((UBITTYPE)1 << (m - 1)))
        ++(*Score);
    else if (Mh & ((UBITTYPE)1 << (m - 1)))
        --(*Score);

    Ph <<= 1;
    Mh <<= 1;
    *Pv = Mh | ~(Xv | Ph);
    *Mv = Ph & Xv;

}

/* Return start position of the match given its end position and score */


    /*
        Here we're guaranteed that score will be decreasing down to:
        1) TargetScore in the case when indexEnd points to the
            string with deletions/modifications from the pattern
        2) 0 in the case when indexEnd points to the string with
            insertions to the end of the pattern
        So we need to search backwards until Score is non-increasing
    */
#define MACRO_MYERS_GET_MATCH_STARTING_POSITION(Mv, Pv, Score, TargetScore, indexEnd)\
    Score = self->m;\
    ScorePrev = self->m;\
    Pv = (UBITTYPE)-1;\
    Mv = (UBITTYPE)0;\
    for (indexStart = indexEnd; indexStart >= 0; --indexStart, ScorePrev = Score)\
    {\
        MACRO_MYERS_CORE_STEP(utext[indexStart], m, self->PEq_R, Mv, Pv, Score)\
        if (Score > ScorePrev && ScorePrev <= TargetScore)\
        {\
            ++indexStart;\
            break;\
        }\
        if (indexStart == 0 && Score <= TargetScore)\
            break;\
    }\
    assert(indexStart >= 0);


static int32_t MyersGetMatchStartingPosition(AgrepCallArgs const* args
    ,int32_t const indexEnd, int32_t const TargetScore
)
{
    MyersSearch const* self = args->self->myers;
    unsigned char const* utext = (unsigned char const*)args->buf;
    int32_t const m = self->m;

    UBITTYPE Pv;
    UBITTYPE Mv;

    int32_t Score, ScorePrev;

    int32_t j;

    Score = m;
    ScorePrev = m;
    Pv = (UBITTYPE)-1;
    Mv = (UBITTYPE)0;
    /*
        Here we're guaranteed that score will be decreasing down to:
        1) TargetScore in the case when indexEnd points to the
            string with deletions/modifications from the pattern
        2) 0 in the case when indexEnd points to the string with
            insertions to the end of the pattern
        So we need to search backwards until Score is non-increasing
    */
    for (j = indexEnd; j >= 0; --j, ScorePrev = Score)
    {
        MyersCoreStep(utext[j], m, self->PEq_R, &Mv, &Pv, &Score);
        SEARCH_DBG("Rvs: %3d. '%c' score %d", j, utext[j], Score);
        if (Score > ScorePrev && ScorePrev <= TargetScore)
        {
            ++j;
            break;
        }
        if (j == 0 && Score <= TargetScore)
            break;
    }
    assert(j >= 0);
    return j;
}

void MyersFindAll(AgrepCallArgs const *args )
{
    MyersSearch const* self = args->self->myers;
    int32_t const threshold = args->threshold;
    const unsigned char *utext = (const unsigned char *)args->buf;
    int32_t const n = args->buflen;
    const void *cbinfo = args->cbinfo;

    AgrepMatch match;
    AgrepContinueFlag cont;

    UBITTYPE Pv;
    UBITTYPE Mv;

    int32_t const m = self->m;
    int32_t Score;

    int32_t j, indexStart;

    Score = m;
    Pv = (UBITTYPE)-1;
    Mv = (UBITTYPE)0;
    
    for (j = 0; j < n; ++j)
    {
        MyersCoreStep(utext[j], m, self->PEq, &Mv, &Pv, &Score);
        /*MACRO_MYERS_CORE_STEP(utext[j], m, self->PEq, Mv, Pv, Score)*/

        if (Score <= threshold)
        {
            indexStart = MyersGetMatchStartingPosition(args, j, Score);
            /*MACRO_MYERS_GET_MATCH_STARTING_POSITION(MvRev, PvRev, ScoreRev, Score, j)*/

            /* found starting point indexStart for current match ending at j with Score */
            /* TODO:
                the algorithm behaves suspiciously on the boundary, e.g.:

                input text: CCGGGTTTTTTAAAAACCCGGAATTCAAACCCTTTCGGTTCCATGTTGAATCACTAGACCCATCATCAGAAACAATTAGCTCATCCTGATCTCGTAAACACCCAAGAATACTATTAATTTGCTCGGCTATATATCTCTCACCATTATAGCTTGCAAGACATACTGATATTTTCACTTTAATATATACTTAATTATATCTGAAGAAGAATATAGATAGTTATTGTCAAAGATAGATCTCATTAGCCTTTTACTAGGGCAAGCTTATATAGTAGTATTTCAAACGGCTTTCTTAAAAAACTTTAGGCCGGGTTTTTTAAAAACCCGGAATTCAAACCCTTTCGGTTCCA
                should find match for GTTGGAACCGAAAGGGTTTGAATTCAAACCCTTTCGGTTCCAAC at pos=6 and pos=310 (len=38) (the same fragment TTTTTTAAAAACCCGGAATTCAAACCCTTTCGGTTCCA) with the same score
                but in fact for pos=310 (310+38=boundary, i.e. the end of line) it finds it with score=13 and for pos=6 - with score=14
            */
            match.score = Score;
            match.position = indexStart;
            match.length = j - indexStart + 1;
            cont = AGREP_CONTINUE;
            (*args->cb)(cbinfo, &match, &cont);
            if (cont != AGREP_CONTINUE)
            {
                return;
            }
        }
    }
}
