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
#include "search-priv.h"
#include <sysalloc.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

/*
  Need tries for large alphabets -- maybe based on hashtable
  of (state, char) pairs.
  Can build alphabet-indexed trie for small alphabets.

  Given a set of strings we're looking for, we have minlength.
  We start looking at curpos + minlength.
*/

struct trie {
    struct trie *next[256];
    const char *s;
#ifdef DEBUG
    char *debugs;
#endif
    int32_t whichpattern;
    int32_t depth;
    int32_t hasmatch; /* Has match below here in the trie. */
};

struct FgrepDumbParams {
    struct trie *trie;
};


#ifdef DEBUG

static
void print_trie_node(struct trie *self, char *car)
{
    printf("%s: %s: %s hasmatch %d\n",
           car, self->debugs, self->s, 0);
}

static
void print_trie(struct trie *self, char *car) 
{
    int32_t i;
    char buf[2];
    for (i=0; i<self->depth; i++) 
        printf(" ");
    print_trie_node(self, car);
    for (i=0; i<256; i++) {
        if (self->next[i] != NULL) {
            buf[0] = i;
            buf[1] = '\0';
            print_trie(self->next[i], buf);
        }
    }
}

#endif /* DEBUG */

static
void free_trie(struct trie *self)
{
    int i;
    for (i=0; i<256; i++) {
        if (self->next[i] != NULL) {
            free_trie(self->next[i]);
            self->next[i] = NULL;
        }
    }
    free(self);
}

static
void trie_enter(struct trie *self, const char *s, int32_t minlen)
{
    struct trie *cur = self;
    struct trie *newone = NULL;
    int32_t len;
    int32_t i;
    len = strlen(s);
    for (i=0; i<len; i++) {
        unsigned char c = s[i];
        if (NULL != cur->next[c]) {
            cur->hasmatch = 1;
            cur = cur->next[c];
        } else {
            newone = (struct trie *)malloc(sizeof(struct trie));
            newone->s = NULL;
#ifdef DEBUG
            newone->debugs = create_substring(s, i+1);
#endif
            newone->depth = i+1;
            newone->hasmatch = 1;
            memset( newone->next, 0, sizeof( newone->next ) );
            cur->next[c] = newone;
            cur = newone;
        }
    }
    cur->s = s;
}


static
void buildtrie(struct trie **self, const char *strings[], int32_t numstrings)
{
    int32_t i;
    int32_t minlen = 1000000;
    int32_t len;

    for (i=0; i<numstrings; i++) {
        len = strlen(strings[i]);
        if (len < minlen) {
            minlen = len;
        }
    }

    *self = (struct trie *)malloc(sizeof(struct trie));
    for (i=0; i<256; i++) {
        (*self)->next[i] = NULL;
    }
    (*self)->s = NULL;
  
    for (i=0; i<numstrings; i++) {
        trie_enter(*self, strings[i], minlen);
    }
}

void FgrepDumbSearchFree( FgrepDumbParams *self )
{
    free_trie(self->trie);
    free(self);
}

void FgrepDumbSearchMake( FgrepDumbParams **self,
        const char *strings[], uint32_t numstrings )
{
    /* int32_t i; */
    struct trie *trie;
    /* int32_t len; */
    buildtrie(&trie, strings, numstrings);
    *self = (FgrepDumbParams *)malloc(sizeof(FgrepDumbParams));
    (*self)->trie = trie;
    /* for (i=0; i<numstrings; i++) {
        len = strlen(strings[i]);
    } */
}

uint32_t FgrepDumbFindFirst( FgrepDumbParams *self,
        const char *buf, size_t len, FgrepMatch *match )
{
    struct trie *trie;
    struct trie *newtrie;
    unsigned char *p = (unsigned char *)buf;
    unsigned char *startp;
    unsigned char *endp = (unsigned char *)buf+len;

    for (startp = (unsigned char *)buf; startp < endp; startp++) {
        p = startp;
        trie = self->trie;
        while (p < endp) {
            newtrie = trie->next[*p++];
            if (newtrie == NULL) {
                break;
            }
            trie = newtrie;
            if (trie->s != NULL) {
                match->position = startp-(unsigned char *)buf;
                match->length = trie->depth;
                match->whichpattern = trie->whichpattern;
                return 1;
            }
        }
    }
    return 0;
}

void FgrepDumbFindAll ( FgrepDumbParams *self,
        char *buf, int32_t len, FgrepMatchCallback cb, void *cbinfo )
{
    unsigned char *ubuf = (unsigned char *)buf;
    struct trie *trie;
    struct trie *newtrie;
    unsigned char *p = ubuf;
    unsigned char *startp;
    unsigned char *endp = ubuf+len;
    FgrepContinueFlag cont;
    FgrepMatch match;

    for (startp = ubuf; startp < endp; startp++) {
        p = startp;
        trie = self->trie;
        while (p < endp) {
            newtrie = trie->next[*p++];
            if (newtrie == NULL) {
                break;
            }
            trie = newtrie;
            if (trie->s != NULL) {
                cont = FGREP_CONTINUE;
                match.position = startp-ubuf;
                match.length = trie->depth;
                match.whichpattern = trie->whichpattern;
                (*cb)(cbinfo, &match, &cont);
                if (cont != FGREP_CONTINUE)
                    return;
            }
        }
    }
}


/* Try the longest match first. */
LIB_EXPORT uint32_t CC has_left_exact_match( char *pattern, char *buf, size_t buflen, 
                              int32_t *length )
{
    int32_t plen = strlen(pattern);
    int32_t i, j;

    /* Added bounds checking -- untested */
    int32_t bound = plen;
    if (buflen < bound)
        bound = buflen;
    
    for (i=bound; i>=1; i--) {
        for (j=0; j<i; j++) {
            if (pattern[plen-i+j] != buf[j] &&
                buf[j] != 'N')
                goto CONT;
        }
        /* Found a match */
        *length = i;
        return 1;
    CONT: {}
    }
    return 0;
}

LIB_EXPORT uint32_t CC has_right_exact_match( char *pattern, char *buf, size_t buflen, 
                               int32_t *bestpos )
{
    int32_t plen = strlen(pattern);
    int32_t bound = plen;
    int32_t i, j;

    if (buflen <= 0)
        return 0;

    if (buflen < bound) 
        bound = buflen;

    /* i is here the length of the prefix */
    for (i=bound; i>=1; i--) {
        for (j=0; j<i; j++) {
            if (pattern[j] != buf[buflen-i+j] &&
                buf[buflen-i+j] != 'N')
                goto CONT;
        }
        /* Found a match */
        *bestpos = buflen-i;
        return 1;
    CONT: {}
    }
    return 0;
}


/* Try the longest match first. */
/* Used to assume that the text to be matched was at least as long as 
the pattern.  No more. 
*/
LIB_EXPORT uint32_t CC has_inside_exact_match( char *pattern, char *buf, size_t buflen, 
                                int32_t *skip )
{
    int32_t plen = strlen(pattern);
    int32_t i, j;

    if (buflen <= 0)
        return 0;

    /* i is here the start in the pattern */
    for (i=1; i<=plen; i++) {
        for (j=0; j<buflen; j++) {
            if (pattern[i+j] != buf[j] &&
                buf[j] != 'N')
                goto CONT;
        }
        /* Found a match */
        *skip = i;
        return 1;
    CONT: {}
    }
    return 0;
}

