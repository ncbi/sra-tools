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

static
char *reverse_string(const char *s) 
{
    int32_t len = strlen(s);
    char *ret = malloc(len+1);
    char *rp = ret;
    const char *src = s+len;
    while (--src >= s) {
        *rp++ = *src;
    }
    *rp = '\0';
    return ret;
}


struct trie {
    struct trie *next[256];
    char *s;
#ifdef DEBUG
    char *debugs;
#endif
    int32_t whichpattern;
    int32_t depth;
    int32_t hasmatch; /* Has match below here in the trie. */
    /*
      Used to be called "contained".  Basically we have a pattern that
      has been matched in the text, so we either need to contain this,
      or use a suffix of this as a prefix of our pattern.

      Any prospective pattern must either have its prefix here,
      or contain what we've seen.
      Note: this will be less than the skip below.
    */
    int32_t minskip_matched; 

    /* 
       The pattern contained unmatched chars.  This used to be called prefix.
       Basically we have some unmatched characters, so it doesn't help to 
       contain this pattern, we need to start our string using some suffix
       of this pattern.  Note: the whole pattern could start a match.

       Any prospective pattern must have its prefix match a suffix of this.
    */
    int32_t minskip_unmatched; 
};

#ifdef DEBUG

static
void print_trie_node(struct trie *self, char *car)
{
    printf("%s: %s: %s hasmatch %d matched %d unmatched %d\n",
           car, self->debugs, self->s, self->hasmatch, self->minskip_matched, self->minskip_unmatched);
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
void trie_enter(struct trie *self, int32_t whichpattern, char *s, int32_t minlen)
{
    struct trie *cur = self;
    struct trie *newone = NULL;
    int32_t len;
    int32_t i;
    len = strlen(s);
    for (i=0; i<len; i++) {
        unsigned char c = (unsigned char)s[i];
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
            newone->minskip_matched = minlen;
            newone->minskip_unmatched = minlen;
            memset( newone->next, 0, sizeof( newone->next ) );
            cur->next[c] = newone;
            cur = newone;
        }
    }
    cur->whichpattern = whichpattern;
    cur->s = s;
}

static
void trie_enter_suffixes(struct trie *self, char *s, int32_t minlen)
{
    struct trie *cur;
    struct trie *newone;
    int32_t suf;
    int32_t i;
    int32_t len;
    len = strlen(s);
    for (suf = 1; suf<len; suf++) {
        cur = self;
        for (i=0; suf+i<len; i++) {
            unsigned char c = s[suf+i];
            if (NULL != cur->next[c]) {
                cur = cur->next[c];
#ifdef NOPE
                if (!cur->hasmatch)
                    break;
#endif
            } else {
                newone = (struct trie *)malloc(sizeof(struct trie));
                newone->s = NULL;
#ifdef DEBUG
                newone->debugs = create_substring(s+suf, i+1);
#endif
                newone->depth = i+1;
                newone->hasmatch = 0;
                newone->minskip_matched = suf;
                newone->minskip_unmatched = minlen;
                memset( newone->next, 0, sizeof( newone->next ) );
                cur->next[c] = newone;
                cur = newone;
            }
        }
    }
}


/* This recurses down the trie and sets the prefix_skip
   based upon what's computer in trie_set_minskip for the prefix skip.
*/
static
void trie_recurse_prefix_minskip(struct trie *trie, int32_t skip) 
{
    int32_t i;
    if (skip < trie->minskip_unmatched) {
#ifdef DEBUG
        if (trie->debugs == NULL) {
            fprintf(stderr, "Setting minskip prefix unmatched of (something) to %d\n", skip);
        } else {
            fprintf(stderr, "Setting minskip prefix unmatched of %s to %d\n", trie->debugs, skip);
        }
#endif
        trie->minskip_unmatched = skip;
    }
    if (skip < trie->minskip_matched) {
#ifdef DEBUG
        if (trie->debugs == NULL) {
            fprintf(stderr, "Setting minskip prefix matched of (something) to %d\n", skip);
        } else {
            fprintf(stderr, "Setting minskip prefix matched of %s to %d\n", trie->debugs, skip);
        }
#endif
        trie->minskip_matched = skip;
    }
    for (i=0; i<256; i++) {
        if (trie->next[i] != NULL) {
            trie_recurse_prefix_minskip(trie->next[i], skip);
        }
    }
}

static
void trie_set_minskip(struct trie *trie, const char *s, const char *rs)
{
    unsigned char *urs = (unsigned char *)rs;
    unsigned char *us = (unsigned char *)s;
    struct trie *cur;
    int32_t len;
    int32_t i;
    int32_t skip;
    char buf[1024];
    int32_t prefix;
    len = strlen(rs);
    for (skip=1; skip<len; skip++) {
        cur = trie->next[urs[skip]];
        for (i=1; i<len-skip && cur != NULL; i++) {
            if (skip < cur->minskip_matched) {
                strncpy(buf, rs+skip, i);
                buf[i] = '\0';
#ifdef DEBUG
                fprintf(stderr, "Setting minskip contained of %s to %d\n", buf, skip);
#endif                    
                cur->minskip_matched = skip;
            }
            cur = cur->next[urs[skip+i]];
        }
    }
    /* Now we need to set up minskip_prefix --
       We need to walk down the tree from any prefix of our word,
       and set the minskip_prefix based upon (wordlen-prefix).
       Note: non-empty prefixes, so start at 1.
    */
    for (prefix = 1; prefix<len; prefix++) {
        skip = len - prefix;
        cur = trie;
        for (i=prefix-1; i>=0 && cur != NULL; i--) {
            cur = cur->next[us[i]];
        }
        if (cur != NULL) {
#ifdef DEBUG
            fprintf(stderr, "Setting minskip to at most %d in:\n", skip);
            print_trie_node(cur, s);
#endif            
            trie_recurse_prefix_minskip(cur, skip);
        }
    }
}

static    
void buildreversetrie(struct trie **self, const char *strings[], int32_t numstrings)
{
    int32_t i;
    int32_t minlen = 1000000;
    int32_t len;
    char *reversestrings[10000];

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
        reversestrings[i] = reverse_string(strings[i]);
    }
    for (i=0; i<numstrings; i++) {
        trie_enter(*self, i, reversestrings[i], minlen);
    }
    for (i=0; i<numstrings; i++) {
        trie_enter_suffixes(*self, reversestrings[i], minlen);
    }
    for (i=0; i<numstrings; i++) {
        trie_set_minskip(*self, strings[i], reversestrings[i]);
    }
    (*self)->minskip_matched = 1;
    (*self)->minskip_unmatched = minlen;
    
    for (i=0; i<numstrings; i++) {
        free ( reversestrings [ i ] );
    }
}



struct FgrepBoyerParams {
    struct trie *trie;
    int32_t charskips[256];
    int32_t minlength;
};


void FgrepBoyerSearchFree( FgrepBoyerParams *self )
{
    free_trie(self->trie);
    free(self);
}


void FgrepBoyerSearchMake ( FgrepBoyerParams **self,
        const char *strings[], uint32_t numstrings )
{
    int32_t i;
    struct trie *trie;
    int32_t len;
    buildreversetrie(&trie, strings, numstrings);
#ifdef DEBUG
    print_trie(trie, "");
#endif
    *self = (FgrepBoyerParams *)malloc(sizeof(FgrepBoyerParams));
    (*self)->trie = trie;
    (*self)->minlength = 10000;
    for (i=0; i<numstrings; i++) {
        len = strlen(strings[i]);
        if (len < (*self)->minlength) {
            (*self)->minlength = len;
        }
    }
}

/*
  Currently implementing for bytes.
  Returns non-negative if found, -1 if not.
*/
uint32_t FgrepBoyerFindFirst ( FgrepBoyerParams *self,
        const char *buf, size_t len, FgrepMatch *match )
{
    unsigned char *ubuf = (unsigned char *)buf;
    struct trie *trie;
    struct trie *newtrie;
    unsigned char *p = ubuf;
    unsigned char *startp;
    unsigned char *endp = ubuf+len;
    int32_t skips = 0;

    /* fprintf(stderr, "Searching %10s... for %d chars\n", buf, len); */
    p = ubuf + self->minlength;
    while (1) {
        if (p >= endp)
            break;
        startp = p;
        trie = self->trie;
    CONT:
        if (NULL == (newtrie = trie->next[*--p])) {
            p = startp + trie->minskip_unmatched;
            skips += trie->minskip_unmatched;
            continue;
        }
        if (!newtrie->hasmatch) {
            while (newtrie) {
                trie = newtrie;
                newtrie = newtrie->next[*--p];
            }
            p = startp + trie->minskip_unmatched;
            skips += trie->minskip_unmatched;
            continue;
        }
        if (newtrie->s != NULL) {
            match->position = p - ubuf;
            match->length = newtrie->depth;
            match->whichpattern = newtrie->whichpattern;
            return 1;
        }
        trie = newtrie;
        goto CONT;
    }
    return 0;
}

void FgrepBoyerFindAll ( FgrepBoyerParams *self,
        char *buf, int32_t len, FgrepMatchCallback cb, void *cbinfo )
{
    unsigned char *ubuf = (unsigned char *)buf;
    struct trie *trie;
    struct trie *newtrie;
    unsigned char *p = ubuf;
    unsigned char *startp;
    unsigned char *endp = ubuf+len;
    int32_t skips = 0;
    FgrepContinueFlag cont;
    FgrepMatch match;

    /* fprintf(stderr, "Searching %10s... for %d chars\n", buf, len); */
    p = ubuf + self->minlength;
    while (1) {
        if (p >= endp)
            break;
        startp = p;
        trie = self->trie;
    CONT:
        if (NULL == (newtrie = trie->next[*--p])) {
            p = startp + trie->minskip_unmatched;
            skips += trie->minskip_unmatched;
            continue;
        }
        if (!newtrie->hasmatch) {
            while (newtrie) {
                trie = newtrie;
                newtrie = newtrie->next[*--p];
            }
            p = startp + trie->minskip_unmatched;
            skips += trie->minskip_unmatched;
            continue;
        }
        if (newtrie->s != NULL) {
            cont = FGREP_CONTINUE;
            match.position = p - ubuf;
            match.length = newtrie->depth;
            match.whichpattern = newtrie->whichpattern;
            (*cb)(cbinfo, &match, &cont);
            if (cont != FGREP_CONTINUE)
                return;
        }
        trie = newtrie;
        goto CONT;
    }
}
