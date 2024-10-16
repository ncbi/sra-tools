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

#include <klib/text.h>
#include <search/extern.h>
#include <compiler.h>
#include <os-native.h>
#include <sysalloc.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "search-priv.h"

static
int32_t debug = 0;

struct AgrepWuParams {
    unsigned char* pattern;
    int32_t len;
    uint64_t alphabits[256];
    uint64_t r_alphabits[256];
    int64_t patmask; /* Actually an inverse mask (0's in the pattern positions) */
};

/* This now computes the alphabits for a 4na pattern. */
static
rc_t compute_alphabits_4na(AgrepWuParams *self, AgrepFlags mode)
{
    rc_t rc = 0;
    int32_t i;

    self->patmask = ((uint64_t) -1) << self->len;

    for(i = 0; i < 256; i++) {
        self->alphabits[i] = self->patmask;
        self->r_alphabits[i] = self->patmask;
    }
    for(i = 0; rc == 0 && i < self->len; i++) {
        if( (rc = na4_set_bits(mode, self->alphabits, self->pattern[i], (uint64_t)1<<(self->len-i-1))) == 0 ) {
            rc = na4_set_bits(mode, self->r_alphabits, self->pattern[self->len-i-1], (uint64_t)1<<(self->len-i-1));
        }
    }
    return rc;
}

static
void compute_alphabits(AgrepWuParams *self)
{
    int32_t i, j;
    uint64_t bits, patmask;

    patmask = self->patmask = ((uint64_t)-1) << self->len;
 
    /* TBD use AGREP_IGNORE_CASE */
    for (i=0; i<256; i++) {
        bits = 0;
        for (j=0; j<self->len; j++) {
            if (self->pattern[j] == i) {
                bits |= (uint64_t)1<<(self->len-j-1);
            }
        }
        self->alphabits[i] = bits | patmask;
    }
    for (i=0; i<256; i++) {
        bits = 0;
        for (j=0; j<self->len; j++) {
            if (self->pattern[self->len-j-1] == i) {
                bits |= (uint64_t)1<<(self->len-j-1);
            }
        }
        self->r_alphabits[i] = bits | patmask;
    }
}

rc_t AgrepWuMake( AgrepWuParams **self, AgrepFlags mode, const char *pattern )
{
    rc_t rc = 0;

    *self = NULL;
    if(strlen(pattern) > 63) {
        rc = RC( rcText, rcString, rcSearching, rcParam, rcExcessive);
    } else if( (*self = malloc(sizeof(**self))) == NULL ) {
        rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
    } else {
        (*self)->pattern = (unsigned char*)string_dup_measure(pattern, NULL);
        (*self)->len = strlen(pattern);
        if( (*self)->pattern == NULL ) {
            rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
        } else if(mode & AGREP_PATTERN_4NA) {
            rc = compute_alphabits_4na(*self, mode);
        } else {
            compute_alphabits(*self);
        }
    }
    if( rc != 0 ) {
        AgrepWuFree(*self);
        *self = NULL;
    }
    return rc;
}

void AgrepWuFree ( AgrepWuParams *self )
{
    if( self != NULL ) {
        free(self->pattern);
        free(self);
    }
}

static
void callback_with_end( const AgrepCallArgs *args )
{
    AgrepFlags mode = args->self->mode;
    AgrepWuParams *self = args->self->wu;
    int32_t threshold = args->threshold;
    unsigned char *buf = (unsigned char *)args->buf;
    int32_t buflen = args->buflen;
    AgrepMatchCallback cb = dp_end_callback;
    const void *cbinfo = args;
    
    AgrepMatch match;
    AgrepContinueFlag cont;
    int32_t i, k;
    int64_t *R = malloc(sizeof(int64_t) * (threshold + 1));
    int64_t *Rnew = malloc(sizeof(int64_t) * (threshold + 1));
    int64_t *tmp;

    int32_t Score;
    int32_t curscore = 0;
    int32_t curlast = 0;
    int32_t continuing = 0;

    for (k=0; k<=threshold; k++) {
        R[k] = self->patmask>>k;
    }

    for (i=0; i<buflen; i++) {
        /* Not with 2na, you don't! 
        if (buf[i] == '\0')
        break; */

        uint64_t bits = self->alphabits[buf[i]];
        Rnew[0] = (R[0] >> 1) & bits;
        for (k=1; k<=threshold; k++) {
            Rnew[k] = ((R[k] >> 1) & bits) | R[k-1]>>1 | Rnew[k-1]>>1 | R[k-1];
        }
        Score = -1;
        for (k=0; k<=threshold; k++) {
            if (Rnew[k] & 1) {
                /* Found a match -- report ending position, use DP for backscan */
                Score = k;
                break;
            }
        }
        if (Score >= 0) {
            if (continuing) {
                if (Score < curscore &&
                    ((mode & AGREP_EXTEND_BETTER) ||
                     (mode & AGREP_EXTEND_SAME))) {
                    curscore = Score;
                    curlast = i;
                } else if (Score == curscore &&
                           ((mode & AGREP_EXTEND_BETTER) ||
                            (mode & AGREP_EXTEND_SAME))) {
                    if (mode & AGREP_EXTEND_SAME) {
                        curlast = i;
                    }
                } else {
                    continuing = 0;
                    match.score = curscore;
                    match.position = curlast;
                    match.length = -1;
                    cont = AGREP_CONTINUE;
                    (*cb)(cbinfo, &match, &cont);
                    if (cont != AGREP_CONTINUE)
                        goto EXIT;
                }
            } else if ((mode & AGREP_EXTEND_SAME) ||
                       (mode & AGREP_EXTEND_BETTER)) {
                curscore = Score;
                curlast = i;
                continuing = 1;
            } else {
                match.score = Score;
                match.position = i;
                match.length = -1;
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
            match.position = curlast;
            match.length = -1;
            cont = AGREP_CONTINUE;
            (*cb)(cbinfo, &match, &cont);
            if (cont != AGREP_CONTINUE)
                goto EXIT;
        }
        tmp = R; R = Rnew; Rnew = tmp;
    }
    if (continuing) {
        continuing = 0;
        match.score = curscore;
        match.position = curlast;
        match.length = -1;
        (*cb)(cbinfo, &match, &cont);
    }
EXIT:
    free(R);
    free(Rnew);
}

/* 
   Returns -1 if no match found, otherwise returns ENDING position.
*/

static
int32_t find_end(const AgrepWuParams *self, int32_t threshold, const char *buf, int32_t buflen, int32_t *score)
{
    const unsigned char *ubuf = (const unsigned char *)buf;
    int32_t i, k;
    int64_t *R = malloc(sizeof(int64_t) * (threshold + 1));
    int64_t *Rnew = malloc(sizeof(int64_t) * (threshold + 1));
    int64_t *tmp;
    int32_t foundit = 0;
    int32_t found = -1; /* This is the value. */
    int32_t stillunderthreshold = 0;
    uint64_t bits;

    *score = threshold;

    for (k=0; k<=threshold; k++) {
        R[k] = self->patmask>>k;
    }

    for (i=0; i<buflen; i++) {

        if (buf[i] == '\0')
            break;
        bits = self->alphabits[ubuf[i]];
        Rnew[0] = (R[0] >> 1) & bits;
        if (Rnew[0] & 1) {
            /* Exact match */
            *score = 0;
            if (debug) {
                printf("Found end match at position %d\n", i);
            }
            free(R);
            free(Rnew);
            return i;
        }
        stillunderthreshold = 0;
        for (k=1; k<=threshold; k++) {

            Rnew[k] = ((R[k] >> 1) & bits) | R[k-1]>>1 | Rnew[k-1]>>1 | R[k-1];

            if (Rnew[k] & 1) {
                stillunderthreshold = 1;
                /* Approx match */
                if (k <= *score) {
                    *score = k;
                    if (debug) {
                        printf("Found approx match at position %d\n", i);
                    }
                    foundit = 1;
                    found = i;
                }
            }
        }
        /* If we're here, we haven't found anything at the threshold we're looking. */
        if (foundit && !stillunderthreshold) {
            free(R);
            free(Rnew);
            return found;
        }
        tmp = R; R = Rnew; Rnew = tmp;
    }
    free(R);
    free(Rnew);
    if (foundit)
        return found;
    return -1;
}

/*
We only need to compute up to the score we're searching for.

Returns nonnegative if found, otherwise returns -1.
*/
static
int32_t find_begin(const AgrepWuParams *self, const char *buf, int32_t buflen, int32_t end, int32_t score )
{
    const unsigned char *ubuf = (const unsigned char *)buf;
    int32_t i, k;
    int64_t *R = malloc(sizeof(int64_t) * (score + 1));
    int64_t *Rnew = malloc(sizeof(int64_t) * (score + 1));
    int64_t *tmp;
    int32_t foundit = 0;
    int32_t found = -1;

    for (k=0; k<=score; k++) {
        R[k] = self->patmask>>k;
    }

    for (i=end; i>=0; i--) {

        uint64_t bits = self->r_alphabits[ubuf[i]];
        Rnew[0] = (R[0] >> 1) & bits;
        if (Rnew[0] & 1) {
            /* Exact match */
            if (debug) {
                printf("Found begin match at position %d\n", i);
            }
            free(R);
            free(Rnew);
            return i;
        }
        for (k=1; k<=score; k++) {

            Rnew[k] = ((R[k] >> 1) & bits) | R[k-1]>>1 | Rnew[k-1]>>1 | R[k-1];

            if (Rnew[k] & 1) {
                /* Approx match */
                if (debug) {
                    printf("Found approx begin match at position %d\n", i);
                }
                score = k; /* Not sure this has an effect */
                foundit = 1;
                found = i;
                goto CONTINUE;
            }
        }
        /* If we're here, we haven't found anything at the threshold we're looking */
        if (foundit) {
            free(R);
            free(Rnew);
            return found;
        }
    CONTINUE:
        tmp = R; R = Rnew; Rnew = tmp;
    }
    free(R);
    free(Rnew);
    if (foundit)
        return found;
    return -1;
}

/* 
Returns nonzero if found something.
*/
uint32_t AgrepWuFindFirst( const AgrepWuParams *self, 
        int32_t threshold, const char *buf, int32_t buflen, AgrepMatch *match )
{
    int32_t end, begin;
    int32_t score;
    while (-1 != (end = find_end(self, threshold, buf, buflen, &score))) {
        begin = find_begin(self, buf, buflen, end, threshold);
        if (begin < 0) {
            /* This is some kind of error condition -- when the reverse search
               goes past the beginning.  I think I wasn't biasing the 
               reverse search, so a "best" reverse search didn't necessarily 
               start at the end of the matching sequence. */
            /* printf("It happened\n"); */
            begin = end-self->len - threshold;
            if (begin < 0) {
                begin = 0;
            }
        }
        match->position = begin;
        match->length = end - begin + 1;
        match->score = score;
        return 1;
    }
    return 0;
}

void AgrepWuFindAll( const AgrepCallArgs *args )
{
    callback_with_end( args );
}
    

