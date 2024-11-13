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

/*
  From Gene Myers, 1998,
  "A Fast Bit-vector Algorithm for Approxsimat String Matching
  Based on Dynamic Programming" -- explains dp pretty well.

  If we place the pattern down the left of a matrix,
  and the scored text along the top horizontally,
  pattern p(length m) and text t(length n),
  we can compute the dynamic programming matrix C[0..m, 0..n] as

  C[i,j] = min{C[i-1,j-1]+(if p(i)=t(j) then 0 else 1), C[i-1,j]+1, C[i,j-1]+1}

  given that C[0,j] = 0 for all j.
  We can replace the constants with table-driven scores,
  so that we have a match cost for (P(i),t(i)),
  and costs for skippings parts of the pattern (first 1) or text (second 1).
*/

#include <search/extern.h>
#include <os-native.h>
#include <compiler.h>
#include <sysalloc.h>
#include <klib/text.h>

#include "search-priv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> /* lroundf */

#define _TRACE 0

static
void reverse_string(const char* from, int32_t len, char *to)
{
    const char* p = from + len;
    while( p != from ) {
        *to++ = *--p;
    }
    *to = '\0';
}

struct DPParams {
    char *pattern;
    char *rpattern;
    AgrepFlags mode;
    int32_t plen;
};

rc_t AgrepDPMake( DPParams **self, AgrepFlags mode, const char *pattern )
{
    rc_t rc = 0;

    if( (*self = malloc(sizeof(**self))) == NULL ) {
        rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
    } else {
        int32_t i;
        (*self)->mode = mode;
        (*self)->pattern = string_dup_measure(pattern, NULL);
        (*self)->plen = strlen(pattern);
        (*self)->rpattern = malloc((*self)->plen + 1);
        if( (*self)->pattern == NULL || (*self)->rpattern == NULL ) {
            rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
        } else if( mode & AGREP_MODE_ASCII ) {
            if( mode & AGREP_IGNORE_CASE ) {
                for(i = 0; i < (*self)->plen; i++) {
                    (*self)->pattern[i] = tolower((*self)->pattern[i]);
                }
            }
        }
    }
    if( rc != 0 ) {
        AgrepDPFree(*self);
    } else {
        reverse_string((*self)->pattern, (*self)->plen, (*self)->rpattern);
    }
    return rc;
}

void AgrepDPFree( DPParams *self )
{
    if ( self != NULL )
    {
        if (self->pattern != NULL)
            free(self->pattern);
        if (self->rpattern != NULL)
            free(self->rpattern);
        free(self);
    }
}

#if _TRACE
static
void print_col_as_row(int32_t *col, int32_t plen)
{
    int32_t i;
    for (i=0; i<=plen; i++) {
        printf("%2d ", col[i]);
    }
    printf("\n");
}
#endif

static
void init_col(const char *p, int32_t plen, int32_t *col)
{
    int32_t i;
    col[0] = 0;
    for (i=1; i<=plen; i++) {
        col[i] = col[i-1] + 1;
    }
}

bool na4_match(unsigned char p, unsigned char c)
{
    if( p == c )
    {
        return true;
    }
    else
    {
        const unsigned char* ps = IUPAC_decode[p];
        const unsigned char* cs = IUPAC_decode[c];

        if ( ps != NULL && cs != NULL )
        {
            size_t i_ps, i_cs;
            for (i_ps = 0; ps[i_ps] != '\0'; ++i_ps)
            {
                for (i_cs = 0; cs[i_cs] != '\0'; ++i_cs)
                {
                    if (ps[i_ps] == cs[i_cs])
                        return true;
                }
            }
        }
    }
    return false;
}

#undef min
#define min(a,b) ((a)<(b)?(a):(b))

/* No longer returns a return code. */
static
void compute_dp_next_col(const char *p, int32_t plen, AgrepFlags mode, int32_t startcost, char t, int32_t *prev, int32_t *nxt)
{
    int32_t i;
    int32_t matchscore = 0;
    nxt[0] = startcost;

    if( (mode & AGREP_TEXT_EXPANDED_2NA) && t < 5 ) {
        t = "ACGTN"[(unsigned char)t];
    }
    for(i = 1; i <= plen; i++)
    {
        if( p[i - 1] == t )
        {
            matchscore = 0;
        }
        else if( mode & AGREP_MODE_ASCII )
        {
            if( mode & AGREP_IGNORE_CASE )
                t = tolower(t);

            matchscore = p[i - 1] == t ? 0 : 1;
        }
        else if( (mode & AGREP_PATTERN_4NA) && na4_match(p[i - 1], t) )
        {
            matchscore = 0;
        }
        else
        {
            matchscore = 1;
        }

        nxt[i] = min(min(prev[i-1] + matchscore,
                         nxt[i-1] + 1),
                     prev[i] + 1);
    }
}

/*
This is new functionality to support partial matches at the beginning
or end of a sequence.
Errors is the number of errors over the whole pattern,
so we can assume a constant error rate, or do something more sophisticated.
Bestpos is the last position that seems to be a match,
and returns the number of "hits" and "misses" where a hit decreases the
edit distance score, and a miss is everything else.

The way to use this is use it in the "forward" direction to find an endpoint,
then assume that's the end of the pattern, and do a reverse search
compare with the (reversed) pattern to see if it really matches
the end of the pattern.  See how it's used in the system for more hints.
*/

LIB_EXPORT void CC dp_scan_for_left_match ( char *pattern, int errors, char *buf,
    int buflen, int *bestpos, int *ret_hits, int *ret_misses )
{
    int plen = strlen(pattern);
    int *prev = malloc(sizeof(int)*(plen+1));
    int *nxt = malloc(sizeof(int)*(plen+1));
    int *tmp;
    int i;

    int patlen = strlen(pattern);
    float errrate = (float)errors / (float)patlen;
    float play;

    int hits = 0;
    int misses = 0;

    int change;

    int lastscore = plen;

    int cont = 1;

    int lastwasmiss = 0;
    int trailing_misses = 0;

    *bestpos = -1;
#if _TRACE
    printf("Err rate is %f\n", errrate);
#endif

    init_col(pattern, plen, nxt);
#if _TRACE
    print_col_as_row(nxt, plen);
#endif
    for (i=0; i<buflen && cont; i++) {
        tmp = prev; prev = nxt; nxt = tmp;
        compute_dp_next_col(pattern, plen, 0, buf[i], i, prev, nxt);

        change = lastscore - nxt[plen];
        if (change == 1) {
            lastwasmiss = 0;
            trailing_misses = 0;
            hits++;
        } else {
            if (lastwasmiss)
                trailing_misses++;
            else
                trailing_misses = 1;
            misses++;
            lastwasmiss = 1;
        }

        play = errrate * (i+1);

        cont = (misses < (1.0+play));

#if _TRACE
        printf("i %d char %c score %d diff %d continue %d misses %d play %f\n",
               i, buf[i], nxt[plen], lastscore - nxt[plen], cont, misses, play);
#endif
        lastscore = nxt[plen];

    }
    /* Settle up */
    i--;
#if _TRACE
    printf("Total hits: %d trailing misses: %d  position: %d\n",
           hits, trailing_misses, i);
#endif
    free(prev);
    free(nxt);

    *bestpos = i - trailing_misses;
    /* Not our usual score. */
    *ret_hits = hits;
    *ret_misses = misses;
}


static
AgrepContinueFlag dp_callback_begin(const AgrepCallArgs *args, int32_t end, int32_t forwardscore)
{
    AgrepFlags mode = args->self->mode;
    char *reverse_pattern = args->self->dp->rpattern;
    int32_t threshold = args->threshold;
    const char *buf = args->buf;
    AgrepMatchCallback cb = args->cb;
    void *cbinfo = args->cbinfo;

    int32_t plen = strlen(reverse_pattern);
    int32_t *prev = malloc(sizeof(int32_t)*(plen+1));
    int32_t *nxt = malloc(sizeof(int32_t)*(plen+1));
    AgrepMatch match;
    AgrepContinueFlag cont;
    int32_t *tmp;
    int32_t i;

    int32_t curscore = 0;
    int32_t curlast = 0;
    int32_t continuing = 0;

    int32_t limit;

    init_col(reverse_pattern, plen, nxt);
#if _TRACE
    print_col_as_row(nxt, plen);
#endif

    limit = end - args->self->dp->plen - threshold - 1;
    if (limit < 0)
        limit = 0;

    for (i=end; i>=limit; i--) {
        tmp = prev; prev = nxt; nxt = tmp;
        /* For the reverse scan, we need to make the initial cost
           of the column depend upon the price of skipping the
           suffix (up to this point) of the text */
        compute_dp_next_col(reverse_pattern, plen, mode, end-i+1,
                            buf[i], prev, nxt);

        if ((mode & AGREP_LEFT_MAINTAIN_SCORE)?
            nxt[plen] <= forwardscore:
            nxt[plen] <= threshold)
        {
            if (continuing) {
                if (nxt[plen] < curscore) {
                    curscore = nxt[plen];
                    curlast = i;
                } else if (nxt[plen] == curscore &&
                           (mode & AGREP_EXTEND_SAME)) {
                    curlast = i;
                } else {
                    continuing = 0;
                    match.score = curscore;
                    match.position = curlast;
                    match.length = end - curlast + 1;
                    cont = AGREP_CONTINUE;
                    (*cb)(cbinfo, &match, &cont);
                    if (cont != AGREP_CONTINUE)
                        goto EXIT;
                }
            } else if ((mode & AGREP_EXTEND_SAME) ||
                       (mode & AGREP_EXTEND_BETTER)) {
                curscore = nxt[plen];
                curlast = i;
                continuing = 1;
            } else {
                match.score = nxt[plen];
                match.position = i;
                match.length = end - i + 1;
                cont = AGREP_CONTINUE;
                (*cb)(cbinfo, &match, &cont);
                if (cont != AGREP_CONTINUE)
                    goto EXIT;
            }
        }
#if _TRACE
        print_col_as_row(nxt, plen);
#endif
    }
    if (continuing) {
        continuing = 0;
        match.score = curscore;
        match.position = curlast;
        match.length = end - curlast + 1;
        cont = AGREP_CONTINUE;
        (*cb)(cbinfo, &match, &cont);
        goto EXIT;
    }
    cont = AGREP_CONTINUE;
EXIT:
    free(prev);
    free(nxt);
    return cont;
}

rc_t CC dp_end_callback( const void *cbinfo, const AgrepMatch *match, AgrepContinueFlag *flag )
{
    const AgrepCallArgs *cbi = cbinfo;
    *flag = dp_callback_begin(cbi, match->position, match->score);
    return 0;
}


static
void dp_callback_end( const AgrepCallArgs *args )
{
    AgrepFlags mode = args->self->mode;
    char *pattern = args->self->dp->pattern;
    int32_t threshold = args->threshold;
    const char *buf = args->buf;
    int32_t buflen = args->buflen;

    AgrepMatchCallback cb = dp_end_callback;
    const void *cbinfo = args;

    int32_t plen = strlen(pattern);
    int32_t *prev = malloc(sizeof(int32_t)*(plen+1));
    int32_t *nxt = malloc(sizeof(int32_t)*(plen+1));
    int32_t curscore = 0;
    int32_t curlast = 0;
    int32_t continuing = 0;

    int32_t startingcost = 0;
    int32_t limit;

    AgrepMatch match;
    AgrepContinueFlag cont;
    int32_t *tmp;
    int32_t i;

    init_col(pattern, plen, nxt);
#if _TRACE
    print_col_as_row(nxt, plen);
#endif

    limit = buflen;
    if (mode & AGREP_ANCHOR_LEFT) {
        limit = args->self->dp->plen + threshold+1;
        if (limit > buflen) {
            limit = buflen;
        }
        cb = args->cb;
        cbinfo = args->cbinfo;
    }

    for (i=0; i<limit; i++) {
        tmp = prev; prev = nxt; nxt = tmp;

        if (mode & AGREP_ANCHOR_LEFT)
            startingcost = i+1;
        compute_dp_next_col(pattern, plen, mode, startingcost,
                            buf[i], prev, nxt);
        if (nxt[plen] <= threshold) {

            if (continuing) {
                if (nxt[plen] < curscore &&
                    ((mode & AGREP_EXTEND_BETTER) ||
                     (mode & AGREP_EXTEND_SAME))) {
                    curscore = nxt[plen];
                    curlast = i;
                } else if (nxt[plen] == curscore &&
                           ((mode & AGREP_EXTEND_BETTER) ||
                            (mode & AGREP_EXTEND_SAME))) {
                    if (mode & AGREP_EXTEND_SAME) {
                        curlast = i;
                    }
                } else {
                    continuing = 0;
                    match.score = curscore;
                    if (mode & AGREP_ANCHOR_LEFT) {
                        match.position = 0;
                        match.length = curlast+1;
                    } else {
                        match.position = curlast;
                        match.length = -1;
                    }
                    cont = AGREP_CONTINUE;
                    (*cb)(cbinfo, &match, &cont);
                    if (cont != AGREP_CONTINUE)
                        goto EXIT;
                }
            } else if ((mode & AGREP_EXTEND_SAME) ||
                       (mode & AGREP_EXTEND_BETTER)) {
                curscore = nxt[plen];
                curlast = i;
                continuing = 1;
            } else {
                match.score = nxt[plen];
                if (mode & AGREP_ANCHOR_LEFT) {
                    match.position = 0;
                    match.length = i+1;
                } else {
                    match.position = i;
                    match.length = -1;
                }
                cont = AGREP_CONTINUE;
                (*cb)(cbinfo, &match, &cont);
                if (cont != AGREP_CONTINUE)
                    goto EXIT;
            }
            /* If we're no longer under the threshold, we might
               have been moving forward looking for a better match
            */
        } else if (continuing) {
            continuing = 0;
            match.score = curscore;
            if (mode & AGREP_ANCHOR_LEFT) {
                match.position = 0;
                match.length = curlast+1;
            } else {
                match.position = curlast;
                match.length = -1;
            }
            cont = AGREP_CONTINUE;
            (*cb)(cbinfo, &match, &cont);
            if (cont != AGREP_CONTINUE)
                goto EXIT;
        }
#if _TRACE
        print_col_as_row(nxt, plen);
#endif
    }
    if (continuing) {
        continuing = 0;
        match.score = curscore;
        if (mode & AGREP_ANCHOR_LEFT) {
            match.position = 0;
            match.length = curlast+1;
        } else {
            match.position = curlast;
            match.length = -1;
        }
        (*cb)(cbinfo, &match, &cont);
    }
EXIT:
    free(prev);
    free(nxt);
}



static
uint32_t dp_find_end(const char *pattern, AgrepFlags mode, int32_t threshold, const char *buf, int32_t buflen, int32_t *bestpos, int32_t *bestscore)
{
    int32_t plen = strlen(pattern);
    int32_t *prev = malloc(sizeof(int32_t)*(plen+1));
    int32_t *nxt = malloc(sizeof(int32_t)*(plen+1));
    int32_t *tmp;
    int32_t i;

    int32_t foundit = 0;


    *bestscore = 10000;
    *bestpos = 1;

    init_col(pattern, plen, nxt);
#if _TRACE
    print_col_as_row(nxt, plen);
#endif
    for (i=0; i<buflen; i++) {
        tmp = prev; prev = nxt; nxt = tmp;
        compute_dp_next_col(pattern, plen, mode, 0, buf[i], prev, nxt);
        if (nxt[plen] <= threshold) {
            if (foundit) {
                if (nxt[plen] <= *bestscore) {
                    *bestpos = i;
                    *bestscore = nxt[plen];
                } else {
                    /* Here we'd extend even if the score was equal, maybe */
                }
            } else {
                /* Ok, we have a match under threshold.
                   Let's continue and see if we can improve on it.
                */
                *bestpos = i;
                *bestscore = nxt[plen];
                foundit = 1;
            }
        } else {
            if (foundit)
                goto EXIT;
        }
#if _TRACE
        print_col_as_row(nxt, plen);
#endif
    }
EXIT:
    free(prev);
    free(nxt);
    if (foundit)
        return 1;
    return 0;
}




static
uint32_t dp_find_begin(char *reverse_pattern, AgrepFlags mode, int32_t threshold, const char *buf, int32_t buflen, int32_t end, int32_t *begin)
{
    int32_t plen = strlen(reverse_pattern);
    int32_t *prev = malloc(sizeof(int32_t)*(plen+1));
    int32_t *nxt = malloc(sizeof(int32_t)*(plen+1));
    int32_t *tmp;
    int32_t i;

    int32_t limit;

    int32_t foundit = 0;
    /* int32_t bestscore = 10000; */

    *begin = 0;

    limit = end - plen - threshold - 1;
    if (limit < 0)
        limit = 0;

    init_col(reverse_pattern, plen, nxt);
#if _TRACE
    print_col_as_row(nxt, plen);
#endif
    for (i=end; i>=limit; i--) {
        tmp = prev; prev = nxt; nxt = tmp;
        /* We need to make the initial cost of this column
           reflect the cost of skipping the suffix (up to this point)
           of the text */
        compute_dp_next_col(reverse_pattern, plen, mode, end-i, buf[i], prev, nxt);

        if (nxt[plen] <= threshold) {
            *begin = i;
            /* bestscore = nxt[plen]; */
            foundit = 1;
        } else {
            if (foundit)
                goto EXIT;
        }
#if _TRACE
        print_col_as_row(nxt, plen);
#endif
    }
EXIT:
    free(prev);
    free(nxt);
    if (foundit)
        return 1;
    return 0;
}


uint32_t AgrepDPFindFirst ( const DPParams *self, int32_t threshold, AgrepFlags mode,
        const char *buf, int32_t buflen, AgrepMatch *match )
{
    int32_t begin, end;
    int32_t score;
    if (dp_find_end(self->pattern, mode, threshold, buf, buflen, &end, &score)) {
        if (dp_find_begin(self->rpattern, mode, threshold, buf, buflen, end, &begin)) {
            match->position = begin;
            match->length = end-begin+1;
            match->score = score;
            return 1;
        }
    }
    return 0;
}


void AgrepDPFindAll( const AgrepCallArgs *args )
{
    dp_callback_end( args );
}


/* Try the longest match first. */
LIB_EXPORT uint32_t CC has_left_approx_match( char *pattern, uint32_t errors,
                               char *buf, size_t buflen,
                               uint32_t *length, uint32_t *errcnt )
{
    int32_t plen = strlen(pattern);
    int32_t *prev = malloc(sizeof(int)*(plen+1));
    int32_t *nxt = malloc(sizeof(int)*(plen+1));
    int32_t *tmp;
    int32_t i, j;
    int32_t allowable;
    char *subpattern;
    int32_t dist;

    int32_t found = 0;
    int32_t foundpos = 0;
    int32_t founderr = 0;

    for (i=plen; i>=8; i--) {

        /* See if the first i chars of the text match the last i
           chars of the pattern with (errors) errors.
        */
        subpattern = pattern + (plen - i);
        init_col(subpattern, i, nxt);
        for (j=0; j<i; j++) {
            tmp = prev; prev = nxt; nxt = tmp;
            compute_dp_next_col(subpattern, i, 0, buf[j], j, prev, nxt);
        }
        dist = nxt[i];
        allowable = 1+lroundf((float)i * (float)errors / (float)plen);
        if (found) {
            if (dist <= founderr) {
                foundpos = i-1;
                founderr = dist;
            } else {
                goto DONE;
            }
        } else if (dist <= allowable) {
            /* Found a match */
            found = 1;
            foundpos = i-1;
            founderr = dist;

            /* Continue a bit to see if something's equally good or better */
        }
    }
    DONE:
    free(prev);
    free(nxt);
    if (found) {
        *length = foundpos+1;
        *errcnt = founderr;
        return 1;
    }
    return 0;
}


/* Try the longest match first. */
LIB_EXPORT uint32_t CC has_right_approx_match( char *pattern, uint32_t errors,
                                char *buf, size_t buflen,
                                uint32_t *bestpos, uint32_t *errcnt )
{
    uint32_t plen = strlen(pattern);
    int32_t *prev = malloc(sizeof(int)*(plen+1));
    int32_t *nxt = malloc(sizeof(int)*(plen+1));
    int32_t *tmp;
    int32_t i, j;
    int32_t allowable;
    char *subpattern, chBackup;
    char *subpattern_r;
    int32_t dist;

    int32_t found = 0;
    int32_t foundpos = 0;
    int32_t founderr = 0;
    int32_t bufj;

    int bound = plen;
    if (buflen < bound) {
        bound = buflen;
    }

    subpattern = malloc(plen + 1);
    subpattern_r = malloc(plen + 1);

    string_copy ( subpattern, plen + 1, pattern, plen );

    for (i=bound; i>=8; i--, subpattern[i] = chBackup) {

        /* See if the first i chars of the pattern match the last i
           chars of the text with (errors) errors.
           We match in reverse, so the initial penalty of skipping
           the "first part" of the pattern means skipping the end
        */
        /* making prefix of length i out of pattern
        (subpattern contains full copy of pattern)*/
        chBackup = subpattern[i];
        subpattern[i] = '\0';

        reverse_string(subpattern, i, subpattern_r);
        init_col(subpattern_r, i, nxt);

        for (j=0; j<i; j++) {
            bufj = buflen - j - 1;
            tmp = prev; prev = nxt; nxt = tmp;
            compute_dp_next_col(subpattern_r, i, 0, buf[bufj], j, prev, nxt);
        }
        dist = nxt[i];
        allowable = 1+lroundf((float)i * (float)errors / (float)plen);
        if (found) {
            if (dist <= founderr) {
                foundpos = buflen - i;
                founderr = dist;
            } else {
                goto DONE;
            }
        } else if (dist <= allowable) {
            /* Found a match */
            found = 1;
            foundpos = buflen - i;
            founderr = dist;

            /* Continue a bit to see if something's equally good or better */
        }
    }
    DONE:
    free(subpattern);
    free(subpattern_r);
    free(prev);
    free(nxt);
    if (found) {
        *bestpos = foundpos;
        *errcnt = founderr;
        return 1;
    }
    return 0;
}



/* Try the longest match first. */
/* Call with pattern as the text, text as pattern. */
LIB_EXPORT uint32_t CC has_inside_approx_match( char *pattern, uint32_t plen, uint32_t errors,
                                 char *buf, size_t buflen,
                                 uint32_t *skip, uint32_t *errcnt )
{
    int32_t *prev = malloc(sizeof(int)*(plen+1));
    int32_t *nxt = malloc(sizeof(int)*(plen+1));
    int32_t *tmp;
    int32_t j;
    int32_t allowable;
    int32_t dist;

    int32_t found = 0;
    int32_t foundpos = 0;
    int32_t founderr = 0;

    allowable = 1+lroundf((float)errors * plen / (float)buflen);


    init_col(pattern, plen, nxt);

    for (j=0; j<buflen; j++) {
        tmp = prev; prev = nxt; nxt = tmp;
        compute_dp_next_col(pattern, plen, 0, buf[j], 0, prev, nxt);

        dist = nxt[plen];

        /*
           We still have to do this kind of thing because otherwise
           the match will extend past the end of the text (here pattern),
           and will match "useless" letters that just increase the score.
           So we continue looking at smaller subsequences of the pattern
           to see if something smaller matches better.
        */

        if (found) {
            if (dist <= founderr && dist <= allowable) {
                foundpos = j;
                founderr = dist;
            } else {
                goto DONE;
            }
        } else if (dist <= allowable) {
            /* Found a match */
            found = 1;
            foundpos = j;
            founderr = dist;

            /* Continue a bit to see if something's equally good or better */
        }
    }
DONE:
    free(prev);
    free(nxt);
    if (found) {
        *skip = foundpos;
        *errcnt = founderr;
        return 1;
    }
    return 0;
}
