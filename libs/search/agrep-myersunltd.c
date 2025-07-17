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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "search-priv.h"


/* 
   We don't use longs here because the addition means we need to keep
   some bits in reserve to calculate carries.
*/   

typedef int32_t schunk;
typedef uint32_t uchunk;

typedef struct CHUNK {
    int32_t size;
    uchunk *chunks;
} CHUNK;

struct MyersUnlimitedSearch {
    int32_t m;
    CHUNK *PEq[256];
    CHUNK *PEq_R[256];
};

static const char NA2KEY [] = "ACGT";
static const char NA4KEY [] = " ACMGRSVTWYHKDBN";

static
int32_t any_non_4na_chars(const char *pattern)
{
    int32_t len = strlen(pattern);
    int32_t i;
    char *p;

    for ( i=0; i<len; i++ )
    {
        p = strchr( NA4KEY, pattern[i] );
        if ( p == NULL )
            return 1;
    }
    return 0;
}

static
int32_t na4key_matches(AgrepFlags mode, char na4, char acgt)
{
    char *p;
    int32_t pos4, pos2;
    p = strchr( NA4KEY, na4 );
    if ( p == NULL )
    {
        p = strchr( NA4KEY, 'N' );
    }
    pos4 = p - NA4KEY;
    pos2 = strchr( NA2KEY, acgt ) - NA2KEY;
    if ( pos4 & ( 1 << pos2 ) )
        return 1;
    return 0;
}




int32_t chunksize(int32_t size) {
    int32_t ret;
    ret = 1 + (size / (8*sizeof(uchunk)));
    return ret;
}

void chunk_or_in(CHUNK *chunk, CHUNK *or)
{
    int32_t i;
    int32_t size = chunk->size;
    for (i=0; i<size; i++)
        chunk->chunks[i] |= or->chunks[i];
}

void chunk_xor_in(CHUNK *chunk, CHUNK *xor)
{
    int32_t i;
    int32_t size = chunk->size;
    for (i=0; i<size; i++)
        chunk->chunks[i] ^= xor->chunks[i];
}


void chunk_and_in(CHUNK *chunk, CHUNK *and)
{
    int32_t i;
    int32_t size = chunk->size;
    for (i=0; i<size; i++)
        chunk->chunks[i] &= and->chunks[i];
}

void chunk_add_in(CHUNK *chunk, CHUNK *add)
{
    int32_t i;
    int32_t size = chunk->size;
    uint64_t carry = 0;
    uint64_t newcarry;
    for (i=size-1; i>=0; i--) {
        newcarry = 
            ((uint64_t)chunk->chunks[i] + (uint64_t)add->chunks[i]
             + carry)
            >> (8*sizeof(uchunk));
        chunk->chunks[i] += add->chunks[i] + carry;
        carry = newcarry;
    }
}


void chunk_set(CHUNK *chunk, CHUNK *copy)
{
    int32_t i;
    for (i=0; i<chunk->size; i++) {
        chunk->chunks[i] = copy->chunks[i];
    }
}


int32_t chunk_isbit_set(CHUNK *chunk, int32_t bit)
{
    int32_t cn = chunk->size - 1 - (bit / (8*sizeof(uchunk)));
    int32_t chunkbit = bit % (8*sizeof(uchunk));
    return chunk->chunks[cn] & (1 << chunkbit);
}


void chunk_set_bit(CHUNK *chunk, int32_t bit) 
{
    int32_t cn = chunk->size - 1 - (bit / (8*sizeof(uchunk)));
    int32_t chunkbit = bit % (8*sizeof(uchunk));
    chunk->chunks[cn] |= (1 << chunkbit);
}


void chunk_negate(CHUNK *chunk)
{
    int32_t i;
    for (i=0; i<chunk->size; i++) {
        chunk->chunks[i] = ~chunk->chunks[i];
    }
}

void alloc_chunk_and_zero(CHUNK *chunk, int32_t size)
{
    chunk->chunks = malloc(size * sizeof(uchunk));
    memset(chunk->chunks, 0, size * sizeof(uchunk));
}

void free_chunk_parts(CHUNK *chunk)
{
    free(chunk->chunks);
    chunk->chunks = NULL;
}

void free_chunk(CHUNK *chunk)
{
    free(chunk->chunks);
    free(chunk);
}


CHUNK *alloc_chunk(int32_t size)
{
    CHUNK *ret;
    ret = malloc(sizeof(CHUNK));
    ret->size = size;
    ret->chunks = malloc(size * sizeof(uchunk));
    memset(ret->chunks, 0, size * sizeof(uchunk));
    return ret;
}
  
void chunk_set_minusone(CHUNK *src)
{
    int32_t i;
    for (i=0; i<src->size; i++) {
        src->chunks[i] = (uchunk)-1;
    }
}

void chunk_zero(CHUNK *src)
{
    int32_t i;
    for (i=0; i<src->size; i++) {
        src->chunks[i] = (uchunk)0;
    }
}

void print_chunk(CHUNK *src)
{
    uchunk chunk;
    unsigned char byte;
    char buf[9];
    int32_t i, j, k;
    buf[8] = '\0';
    for (i=0; i<src->size; i++) {
        chunk = src->chunks[i];
        for (j=0; j<sizeof(uchunk); j++) {
            byte = chunk >> (8*(sizeof(uchunk) - j - 1));
            for (k=0; k<8; k++) {
                buf[k] = "01"[1&(byte>>(7-k))];
            }
            printf("%s ", buf);
        }
    }
    printf("\n");
}


/*
  This currently does not preserve the sign bit.
*/
void lshift_chunk(CHUNK *dest, CHUNK *src, int32_t n)
{
    int32_t i, j;
    uchunk slop;
    int32_t chunkshift = n / (8*sizeof(uchunk));
    int32_t rem = n % (8*sizeof(uchunk));
    /* Assumes they're both the same size. */
    int32_t size = src->size;


    /* i is the destination chunk */
    slop = 0;
    for (i=size-1; i>=0; i--) {
        j = i+chunkshift;
        if (j >= size) {
            dest->chunks[i] = 0;
        } else {
            dest->chunks[i] = slop | src->chunks[j] << rem;
            slop = src->chunks[j] >> (8*sizeof(uchunk) - rem);
        }
    }
}

void chunk_lshift_one_inplace(CHUNK *dest)
{
    int32_t i;
    uchunk slop, newslop;
    int32_t size = dest->size;

    slop = 0;
    for (i=size-1; i>=0; i--) {
        newslop = dest->chunks[i] >> (8*sizeof(uchunk) - 1);
        dest->chunks[i] = slop | dest->chunks[i] << 1;
        slop = newslop;
    }
}


void chunk_lshift_one(CHUNK *dest, CHUNK *src)
{
    int32_t i;
    uchunk slop;
    int32_t size = src->size;

    slop = 0;
    for (i=size-1; i>=0; i--) {
        dest->chunks[i] = slop | src->chunks[i] << 1;
        slop = src->chunks[i] >> (8*sizeof(uchunk) - 1);
    }
}



#ifdef UNUSED_MAY_NOT_WORK  
  
/*
 * In this, chunks are big-endian, meaning 0 is the most significant chunk.
 */
void rshift_uchunk(CHUNK *dest, CHUNK *src, int32_t n)
{
    int32_t size = src->size;
    uchunk slop;
    int32_t chunkshift = n / (8*sizeof(uchunk));
    int32_t rem = n % (8*sizeof(uchunk));
    int32_t i, j;

    /* i is the destination chunk */
    slop = 0;
    for (i=0; i<size; i++) {
        j = i-chunkshift;
        if (j < 0) {
            dest->chunks[i] = 0;
        } else {
            dest->chunks[i] = slop | src->chunks[j] >> rem;
            slop = src->chunks[j] << (8*sizeof(uchunk) - rem);
        }
    }
}

void rshift_schunk(CHUNK *dest, CHUNK *src, int32_t n)
{
    uchunk slop;
    int32_t chunkshift = n / (8*sizeof(uchunk));
    int32_t rem = n % (8*sizeof(uchunk));
    int32_t i, j, size;

    /* i is the destination chunk */
    slop = -1;
    for (i=0; i<size; i++) {
        j = i-chunkshift;
        if (j < 0) {
            dest->chunks[i] = -1;
        } else {
            dest->chunks[i] = slop | src->chunks[j] >> rem;
            slop = src->chunks[j] << (8*sizeof(uchunk) - rem);
        }
    }
}

#endif


void MyersUnlimitedFree ( MyersUnlimitedSearch *self )
{
    int32_t j;
    for (j=0; j<256; j++) {
        free_chunk(self->PEq[j]);
        free_chunk(self->PEq_R[j]);
    }
    free(self);
}
  

rc_t MyersUnlimitedMake ( MyersUnlimitedSearch **self,
        AgrepFlags mode, const char *pattern )
{
    const unsigned char *upattern = (const unsigned char *)pattern;
    int32_t rc;
    int32_t len = strlen(pattern);
    int32_t plen = len;
    int32_t i, j;
    int32_t m;
    int32_t chunks;

    if (!(mode & AGREP_ANYTHING_ELSE_IS_N) &&
        any_non_4na_chars(pattern))
        /* This should be a return code. */
        return RC( rcText, rcString, rcSearching, rcParam, rcUnrecognized);


    *self = malloc(sizeof(MyersUnlimitedSearch));
    m = (*self)->m = len;
    chunks = chunksize(m);
    for (j=0; j<256; j++) {
        (*self)->PEq[j] = alloc_chunk(chunks);
        (*self)->PEq_R[j] = alloc_chunk(chunks);
    }

    for(j = 0; j < m; j++) {
        chunk_set_bit((*self)->PEq[upattern[j]], j);
        if( pattern[j] == 'a' ) {
            chunk_set_bit((*self)->PEq['t'], j); /* t == a */
        }
    }

    for(j = 0; j < m; j++) {
        chunk_set_bit((*self)->PEq_R[upattern[m-j-1]], j);
        if( pattern[m-j-1] == 'a' ) {
            chunk_set_bit((*self)->PEq_R['t'], j); /* t == a */
        }
    }

    for (i=0; i<4; i++) {
        unsigned char acgt = NA2KEY[i];
        for (j=0; j<plen; j++) {
            if (na4key_matches(mode, pattern[j], acgt)) {
                /* bits |= (unsigned long)1<<j; */
                chunk_set_bit((*self)->PEq[acgt], j);
                if (mode & AGREP_TEXT_EXPANDED_2NA)
                    chunk_set_bit((*self)->PEq[i], j);
            }
        }
    }
    for (i=0; i<4; i++) {
        unsigned char acgt = NA2KEY[i];
        for (j=0; j<plen; j++) {
            if (na4key_matches(mode, pattern[plen-j-1], acgt)) {
                chunk_set_bit((*self)->PEq_R[acgt], j);
                if (mode & AGREP_TEXT_EXPANDED_2NA)
                    chunk_set_bit((*self)->PEq_R[i], j);
            }
        }
    }
    return 0;

    for (j=0; j<256; j++) {
        free_chunk((*self)->PEq[j]);
        free_chunk((*self)->PEq_R[j]);
    }
    free(*self);
    *self = NULL;
    return rc;

}


/* 
   This finds the first match forward in the string less than or equal
   the threshold.  If nonzero, this will be a prefix of any exact match.
*/
uint32_t MyersUnlimitedFindFirst ( MyersUnlimitedSearch *self, 
        int32_t threshold, const char* text, size_t n, AgrepMatch *match )
{
    const unsigned char *utext = (const unsigned char *)text;
    CHUNK *Pv;
    CHUNK *Mv;
    CHUNK *Xv, *Xh, *Ph, *Mh;

    int32_t m = self->m;
    int32_t csize = chunksize(m);
    int32_t Score;
    int32_t BestScore = m;
    int32_t from = 0;
    int32_t to = -1;

    int32_t j;

    CHUNK *Eq;

    Pv = alloc_chunk(csize);
    Mv = alloc_chunk(csize);
    Xv = alloc_chunk(csize);
    Xh = alloc_chunk(csize);
    Ph = alloc_chunk(csize);
    Mh = alloc_chunk(csize);


    Score = m;
    chunk_set_minusone(Pv);
    chunk_zero(Mv);
    
    for(j = 0; j < n; j++) {
#ifdef DEBUG
        printf("%d j loop\n", j);
#endif
        Eq = self->PEq[utext[j]];
#ifdef DEBUG
        printf("Eq: "); print_chunk(Eq); printf("\n");
#endif
        /* Xv = Eq | Mv; */
        chunk_set(Xv, Eq);
        chunk_or_in(Xv, Mv);
#ifdef DEBUG
        printf("Xv: "); print_chunk(Xv); printf("\n");
#endif
        /* Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq; */
        chunk_set(Xh, Eq);
        chunk_and_in(Xh, Pv);
        chunk_add_in(Xh, Pv);
        chunk_xor_in(Xh, Pv);
        chunk_or_in(Xh, Eq);
#ifdef DEBUG
        printf("Xh: "); print_chunk(Xh); printf("\n");
#endif
        /* Ph = Mv | ~ (Xh | Pv); */
        chunk_set(Ph, Xh);
        chunk_or_in(Ph, Pv);
        chunk_negate(Ph);
        chunk_or_in(Ph, Mv);
#ifdef DEBUG
        printf("Ph: "); print_chunk(Ph); printf("\n");
#endif
        /* Mh = Pv & Xh; */
        chunk_set(Mh, Pv);
        chunk_and_in(Mh, Xh);
#ifdef DEBUG
        printf("Mh: "); print_chunk(Mh); printf("\n");
#endif
        /* Ph & (1 << (m - 1)) */
        if (chunk_isbit_set(Ph, m-1)) {
            Score++;
            /* Mh & (1 << (m - 1)) */
        } else if (chunk_isbit_set(Mh, m-1)) {
            Score--;
        }
        /* Ph <<= 1; */
        chunk_lshift_one_inplace(Ph);
#ifdef DEBUG
        printf("Ph: "); print_chunk(Ph); printf("\n");
#endif
        /* Mh <<= 1; */
        chunk_lshift_one_inplace(Mh);
#ifdef DEBUG
        printf("Mh: "); print_chunk(Mh); printf("\n");
#endif
        /* Pv = Mh | ~(Xv | Ph); */
        chunk_set(Pv, Xv);
        chunk_or_in(Pv, Ph);
        chunk_negate(Pv);
        chunk_or_in(Pv, Mh);
#ifdef DEBUG
        printf("Pv: "); print_chunk(Pv); printf("\n");
#endif
        /* Mv = Ph & Xv; */
        chunk_set(Mv, Ph);
        chunk_and_in(Mv, Xv);
#ifdef DEBUG
        printf("Mv: "); print_chunk(Mv); printf("\n");
#endif
#ifdef DEBUG
        printf("%3d. score %d\n", j, Score);
#endif
        if (Score <= threshold) {
            BestScore = Score;
            to = j;
            break;
        }
    }

    /* Continue while score decreases under the threshold */
    for(j++; j < n; j++) {
        Eq = self->PEq[utext[j]];
        /* Xv = Eq | Mv; */
        chunk_set(Xv, Eq);
        chunk_or_in(Xv, Mv);
        /* Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq; */
        chunk_set(Xh, Eq);
        chunk_and_in(Xh, Pv);
        chunk_add_in(Xh, Pv);
        chunk_xor_in(Xh, Pv);
        chunk_or_in(Xh, Eq);
        /* Ph = Mv | ~ (Xh | Pv); */
        chunk_set(Ph, Xh);
        chunk_or_in(Ph, Pv);
        chunk_negate(Ph);
        chunk_or_in(Ph, Mv);
        /* Mh = Pv & Xh; */
        chunk_set(Mh, Pv);
        chunk_and_in(Mh, Xh);
        /* Ph & (1 << (m - 1)) */
        if (chunk_isbit_set(Ph, m-1)) {
            Score++;
            /* Mh & (1 << (m - 1)) */
        } else if (chunk_isbit_set(Mh, m-1)) {
            Score--;
        }
        /* Ph <<= 1; */
        chunk_lshift_one_inplace(Ph);
        /* Mh <<= 1; */
        chunk_lshift_one_inplace(Mh);
        /* Pv = Mh | ~(Xv | Ph); */
        chunk_set(Pv, Xv);
        chunk_or_in(Pv, Ph);
        chunk_negate(Pv);
        chunk_or_in(Pv, Mh);
        /* Mv = Ph & Xv; */
        chunk_set(Mv, Ph);
        chunk_and_in(Mv, Xv);

#ifdef DEBUG	
        printf("%3d. score %d\n", j, Score);
#endif
        if (Score < BestScore) {
            BestScore = Score;
            to = j;
        } else {
            break;
        }
    }

    /* Re-initialize for next scan! */
    Score = m;
    chunk_set_minusone(Pv);
    chunk_zero(Mv);
    
    for(j = to; j >= 0; j--) {
        Eq = self->PEq_R[utext[j]]; 	/* This line is different. */
        /* Xv = Eq | Mv; */
        chunk_set(Xv, Eq);
        chunk_or_in(Xv, Mv);
        /* Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq; */
        chunk_set(Xh, Eq);
        chunk_and_in(Xh, Pv);
        chunk_add_in(Xh, Pv);
        chunk_xor_in(Xh, Pv);
        chunk_or_in(Xh, Eq);
        /* Ph = Mv | ~ (Xh | Pv); */
        chunk_set(Ph, Xh);
        chunk_or_in(Ph, Pv);
        chunk_negate(Ph);
        chunk_or_in(Ph, Mv);
        /* Mh = Pv & Xh; */
        chunk_set(Mh, Pv);
        chunk_and_in(Mh, Xh);
        /* Ph & (1 << (m - 1)) */
        if (chunk_isbit_set(Ph, m-1)) {
            Score++;
            /* Mh & (1 << (m - 1)) */
        } else if (chunk_isbit_set(Mh, m-1)) {
            Score--;
        }
        /* Ph <<= 1; */
        chunk_lshift_one_inplace(Ph);
        /* Mh <<= 1; */
        chunk_lshift_one_inplace(Mh);
        /* Pv = Mh | ~(Xv | Ph); */
        chunk_set(Pv, Xv);
        chunk_or_in(Pv, Ph);
        chunk_negate(Pv);
        chunk_or_in(Pv, Mh);
        /* Mv = Ph & Xv; */
        chunk_set(Mv, Ph);
        chunk_and_in(Mv, Xv);

#ifdef DEBUG	
        printf("%3d. score %d\n", j, Score); 
#endif
        if(Score <= BestScore) {
#ifdef DEBUG	    
            printf("rev match at position %d\n", j);
#endif
            from = j;
            break;
        }
    }

    free_chunk(Pv);
    free_chunk(Mv);
    free_chunk(Xv);
    free_chunk(Xh);
    free_chunk(Ph);
    free_chunk(Mh);
    
    if (BestScore <= threshold) {
        match->position = from;
        match->length = to-from+1;
        match->score = BestScore;
        return 1;
    } else {
        return 0;
    }
}

/* 
   This finds the first match forward in the string less than or equal
   the threshold.  If nonzero, this will be a prefix of any exact match.
*/
void MyersUnlimitedFindAll ( const AgrepCallArgs *args )
{
    AgrepFlags mode = args->self->mode;
    MyersUnlimitedSearch *self = args->self->myersunltd;
    int32_t threshold = args->threshold;
    const unsigned char *utext = (const unsigned char *)args->buf;
    int32_t n = args->buflen;
    AgrepMatchCallback cb = dp_end_callback;
    const void *cbinfo = args;

    AgrepMatch match;
    AgrepContinueFlag cont;

    CHUNK *Pv;
    CHUNK *Mv;
    CHUNK *Xv, *Xh, *Ph, *Mh;

    int32_t m = self->m;
    int32_t csize = chunksize(m);
    int32_t Score;

    int32_t curscore = 0;
    int32_t curlast = 0;
    int32_t continuing = 0;

    int32_t j;

    CHUNK *Eq;



    Pv = alloc_chunk(csize);
    Mv = alloc_chunk(csize);
    Xv = alloc_chunk(csize);
    Xh = alloc_chunk(csize);
    Ph = alloc_chunk(csize);
    Mh = alloc_chunk(csize);

    Score = m;
    chunk_set_minusone(Pv);
    chunk_zero(Mv);
    
    for(j = 0; j < n; j++) {
#ifdef DEBUG
        printf("%d j loop\n", j);
#endif
        Eq = self->PEq[utext[j]];
#ifdef DEBUG
        printf("Eq: "); print_chunk(Eq); printf("\n");
#endif
        /* Xv = Eq | Mv; */
        chunk_set(Xv, Eq);
        chunk_or_in(Xv, Mv);
#ifdef DEBUG
        printf("Xv: "); print_chunk(Xv); printf("\n");
#endif
        /* Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq; */
        chunk_set(Xh, Eq);
        chunk_and_in(Xh, Pv);
        chunk_add_in(Xh, Pv);
        chunk_xor_in(Xh, Pv);
        chunk_or_in(Xh, Eq);
#ifdef DEBUG
        printf("Xh: "); print_chunk(Xh); printf("\n");
#endif
        /* Ph = Mv | ~ (Xh | Pv); */
        chunk_set(Ph, Xh);
        chunk_or_in(Ph, Pv);
        chunk_negate(Ph);
        chunk_or_in(Ph, Mv);
#ifdef DEBUG
        printf("Ph: "); print_chunk(Ph); printf("\n");
#endif
        /* Mh = Pv & Xh; */
        chunk_set(Mh, Pv);
        chunk_and_in(Mh, Xh);
#ifdef DEBUG
        printf("Mh: "); print_chunk(Mh); printf("\n");
#endif
        /* Ph & (1 << (m - 1)) */
        if (chunk_isbit_set(Ph, m-1)) {
            Score++;
            /* Mh & (1 << (m - 1)) */
        } else if (chunk_isbit_set(Mh, m-1)) {
            Score--;
        }
        /* Ph <<= 1; */
        chunk_lshift_one_inplace(Ph);
#ifdef DEBUG
        printf("Ph: "); print_chunk(Ph); printf("\n");
#endif
        /* Mh <<= 1; */
        chunk_lshift_one_inplace(Mh);
#ifdef DEBUG
        printf("Mh: "); print_chunk(Mh); printf("\n");
#endif
        /* Pv = Mh | ~(Xv | Ph); */
        chunk_set(Pv, Xv);
        chunk_or_in(Pv, Ph);
        chunk_negate(Pv);
        chunk_or_in(Pv, Mh);
#ifdef DEBUG
        printf("Pv: "); print_chunk(Pv); printf("\n");
#endif
        /* Mv = Ph & Xv; */
        chunk_set(Mv, Ph);
        chunk_and_in(Mv, Xv);
#ifdef DEBUG
        printf("Mv: "); print_chunk(Mv); printf("\n");
#endif
#ifdef DEBUG
        printf("%3d. score %d\n", j, Score);
#endif
        if (Score <= threshold) {
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
                        goto EXIT;
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
        /* print_col_as_row(nxt, plen); */
    }
    if (continuing) {
        continuing = 0;
        match.score = curscore;
        match.position = curlast;
        match.length = -1;
        (*cb)(cbinfo, &match, &cont);
    }

EXIT:

    free_chunk(Pv);
    free_chunk(Mv);
    free_chunk(Xv);
    free_chunk(Xh);
    free_chunk(Ph);
    free_chunk(Mh);
}


LIB_EXPORT int32_t CC MyersUnlimitedFindBest ( MyersUnlimitedSearch *self,
        const char* text, size_t n, int32_t *pos, int32_t *len )
{
    const unsigned char *utext = (const unsigned char *)text;
    CHUNK *Pv;
    CHUNK *Mv;
    CHUNK *Xv, *Xh, *Ph, *Mh;

    int32_t m = self->m;
    int32_t csize = chunksize(m);
    int32_t Score;
    int32_t BestScore = m;
    int32_t from = 0;
    int32_t to = -1;

    int32_t j;

    CHUNK *Eq;

    Pv = alloc_chunk(csize);
    Mv = alloc_chunk(csize);
    Xv = alloc_chunk(csize);
    Xh = alloc_chunk(csize);
    Ph = alloc_chunk(csize);
    Mh = alloc_chunk(csize);


    Score = m;
    chunk_set_minusone(Pv);
    chunk_zero(Mv);
    
    for(j = 0; j < n; j++) {
#ifdef DEBUG
        printf("%d j loop\n", j);
#endif
        Eq = self->PEq[utext[j]];
#ifdef DEBUG
        printf("Eq: "); print_chunk(Eq); printf("\n");
#endif
        /* Xv = Eq | Mv; */
        chunk_set(Xv, Eq);
        chunk_or_in(Xv, Mv);
#ifdef DEBUG
        printf("Xv: "); print_chunk(Xv); printf("\n");
#endif
        /* Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq; */
        chunk_set(Xh, Eq);
        chunk_and_in(Xh, Pv);
        chunk_add_in(Xh, Pv);
        chunk_xor_in(Xh, Pv);
        chunk_or_in(Xh, Eq);
#ifdef DEBUG
        printf("Xh: "); print_chunk(Xh); printf("\n");
#endif
        /* Ph = Mv | ~ (Xh | Pv); */
        chunk_set(Ph, Xh);
        chunk_or_in(Ph, Pv);
        chunk_negate(Ph);
        chunk_or_in(Ph, Mv);
#ifdef DEBUG
        printf("Ph: "); print_chunk(Ph); printf("\n");
#endif
        /* Mh = Pv & Xh; */
        chunk_set(Mh, Pv);
        chunk_and_in(Mh, Xh);
#ifdef DEBUG
        printf("Mh: "); print_chunk(Mh); printf("\n");
#endif
        /* Ph & (1 << (m - 1)) */
        if (chunk_isbit_set(Ph, m-1)) {
            Score++;
            /* Mh & (1 << (m - 1)) */
        } else if (chunk_isbit_set(Mh, m-1)) {
            Score--;
        }
        /* Ph <<= 1; */
        chunk_lshift_one_inplace(Ph);
#ifdef DEBUG
        printf("Ph: "); print_chunk(Ph); printf("\n");
#endif
        /* Mh <<= 1; */
        chunk_lshift_one_inplace(Mh);
#ifdef DEBUG
        printf("Mh: "); print_chunk(Mh); printf("\n");
#endif
        /* Pv = Mh | ~(Xv | Ph); */
        chunk_set(Pv, Xv);
        chunk_or_in(Pv, Ph);
        chunk_negate(Pv);
        chunk_or_in(Pv, Mh);
#ifdef DEBUG
        printf("Pv: "); print_chunk(Pv); printf("\n");
#endif
        /* Mv = Ph & Xv; */
        chunk_set(Mv, Ph);
        chunk_and_in(Mv, Xv);
#ifdef DEBUG
        printf("Mv: "); print_chunk(Mv); printf("\n");
#endif
#ifdef DEBUG
        printf("%3d. score %d\n", j, Score);
#endif
        if (Score < BestScore) {
            BestScore = Score;
            to = j;
        }
    }

    /* Re-initialize for next scan! */
    Score = m;
    chunk_set_minusone(Pv);
    chunk_zero(Mv);
    
    for(j = to; j >= 0; j--) {
        Eq = self->PEq_R[utext[j]]; 	/* This line is different. */
        /* Xv = Eq | Mv; */
        chunk_set(Xv, Eq);
        chunk_or_in(Xv, Mv);
        /* Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq; */
        chunk_set(Xh, Eq);
        chunk_and_in(Xh, Pv);
        chunk_add_in(Xh, Pv);
        chunk_xor_in(Xh, Pv);
        chunk_or_in(Xh, Eq);
        /* Ph = Mv | ~ (Xh | Pv); */
        chunk_set(Ph, Xh);
        chunk_or_in(Ph, Pv);
        chunk_negate(Ph);
        chunk_or_in(Ph, Mv);
        /* Mh = Pv & Xh; */
        chunk_set(Mh, Pv);
        chunk_and_in(Mh, Xh);
        /* Ph & (1 << (m - 1)) */
        if (chunk_isbit_set(Ph, m-1)) {
            Score++;
            /* Mh & (1 << (m - 1)) */
        } else if (chunk_isbit_set(Mh, m-1)) {
            Score--;
        }
        /* Ph <<= 1; */
        chunk_lshift_one_inplace(Ph);
        /* Mh <<= 1; */
        chunk_lshift_one_inplace(Mh);
        /* Pv = Mh | ~(Xv | Ph); */
        chunk_set(Pv, Xv);
        chunk_or_in(Pv, Ph);
        chunk_negate(Pv);
        chunk_or_in(Pv, Mh);
        /* Mv = Ph & Xv; */
        chunk_set(Mv, Ph);
        chunk_and_in(Mv, Xv);

#ifdef DEBUG
        printf("%3d. score %d\n", j, Score); 
#endif
        if(Score <= BestScore) {
#ifdef DEBUG
            printf("rev match at position %d\n", j);
#endif
            from = j;
            break;
        }
    }

    free_chunk(Pv);
    free_chunk(Mv);
    free_chunk(Xv);
    free_chunk(Xh);
    free_chunk(Ph);
    free_chunk(Mh);
    
    *pos = from;
    *len = to-from+1;

    return BestScore;

    /* printf("Found [%d,%d]\n", from, to);
       printf("Found '%.*s'\n", to - from + 1, &text[from]); */
}

