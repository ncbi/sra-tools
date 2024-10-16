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

struct FgrepAhoParams {
    struct trie *trie;
    int32_t charskips[256];
    int32_t minlength;
};

/*
  Need tries for large alphabets -- maybe based on hashtable
  of (state, char) pairs.
  Can build alphabet-indexed trie for small alphabets.

  Given a set of strings we're looking for, we have minlength.
  We start looking at curpos + minlength.
*/

static int32_t quiet = 1;

typedef struct out_s {
    const char *s;
    int32_t whichpattern;
    struct out_s *nxt;
} out_s;

void push_out(out_s **where, const char *out, int32_t whichpattern)
{
    out_s *newout = malloc(sizeof(out_s));
    newout->s = out;
    newout->whichpattern = whichpattern;
    newout->nxt = *where;
    *where = newout;
}
    
static void free_out ( out_s* self )
{
    if ( self != NULL )
    {
        out_s* next = self -> nxt;
        free ( self );
        free_out ( next );
    }
}

struct trie {
    struct trie *next[256];  
    struct trie *fail; /* New for aho-corasick */
    out_s *outs;
    char *debugs;
    int32_t depth;
    int32_t hasmatch; /* Has match below here in the trie. */
};

static
void print_trie_node(struct trie *self, char *car)
{
    out_s *outs;
    if (!quiet) {
        printf("%s: %s: outputs ", car, self->debugs);
        outs = self->outs;
        while (outs != NULL) {
            printf("%s ", outs->s);
            outs = outs->nxt;
        }
    }
}

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
    free_out ( self -> outs );
    free ( self -> debugs );
    free ( self );
}

static
void print_trie(struct trie *self, char *car) 
{
    int32_t i;
    char buf[2];
    if (!quiet) {
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
}

static
void trie_enter(struct trie *self, int32_t whichpattern, const char *s, int32_t minlen)
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
            newone->fail = NULL;
            newone->outs = NULL;
            newone->debugs = create_substring(s, i+1);
            newone->depth = i+1;
            newone->hasmatch = 1;
            memset( newone->next, 0, sizeof( newone->next ) );
            cur->next[c] = newone;
            cur = newone;
        }
    }
    push_out(&cur->outs, s, whichpattern);
}

static
void trie_calc_failure_links(struct trie *self)
{
    struct queue_s {
        struct trie *trie;
        struct queue_s *nxt;
    };
    struct queue_s *queue = NULL;
    struct queue_s *queueend = NULL;
    struct queue_s *newq, *tmpqueue;
    struct trie *r, *u, *v;
    struct trie *trie;
    int32_t i;
    out_s *outs;

    for (i = 0; i<256; i++) {
        if ((trie = self->next[i]) != NULL) {
            trie->fail = self;
            newq = malloc(sizeof(struct queue_s));
            newq->nxt = NULL;
            newq->trie = trie;
            if (queueend != NULL) {
                queueend->nxt = newq;
                queueend = newq;
            } else {
                queue = queueend = newq;
            }
        }
    }
    while (queue != NULL) {
        r = queue->trie;
        tmpqueue = queue;
        queue = queue->nxt;
        free(tmpqueue);
        if (queueend == tmpqueue) {
            queueend = NULL;
        }

        if (r == NULL) {
            printf("It happened.\n");
            continue;
        }
        
        for (i = 0; i<256; i++) {
            if ((u = r->next[i]) != NULL) {            
                newq = malloc(sizeof(struct queue_s));
                newq->trie = u;
                newq->nxt = NULL;
                if (queueend != NULL) {
                    queueend->nxt = newq;
                    queueend = newq;
                } else {
                    queue = queueend = newq;
                }
                /* Not sure about this stuff here. */
                v = r->fail;
                while (v != NULL && v->next[i] == NULL) {
                    v = v->fail;
                }
                if (v != NULL && v->next[i] != NULL) {
                    u->fail = v->next[i];
                } else {
                    u->fail = self;
                }
                outs = u->fail->outs;
                while (outs != NULL) {
                    push_out(&trie->outs, outs->s, outs->whichpattern);
                    outs = outs->nxt;
                }
            }
        }
    }
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
    (*self)->fail = NULL;
    (*self)->outs = NULL;
    (*self)->debugs = NULL;
    (*self)->depth = 0;
    (*self)->hasmatch = 0;
  
    for (i=0; i<numstrings; i++) {
        trie_enter(*self, i, strings[i], minlen);
    }
    trie_calc_failure_links(*self);
}

void FgrepAhoFree( FgrepAhoParams *self )
{
    free_trie(self->trie);
    free(self);
}

void FgrepAhoMake ( FgrepAhoParams **self,
        const char *strings[], uint32_t numstrings )
{
    int32_t i;
    struct trie *trie;
    int32_t len;
    buildtrie(&trie, strings, numstrings);
    if (!quiet) {
        print_trie(trie, "");
    }
    *self = (FgrepAhoParams *)malloc(sizeof(FgrepAhoParams));
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
Returns nonzero when found something.
*/  
uint32_t FgrepAhoFindFirst ( FgrepAhoParams *self,
        const char *buf, size_t len, FgrepMatch *match )
{
    unsigned char *ubuf = (unsigned char *)buf;
    struct trie *trie;
    struct trie *newtrie;
    
    unsigned char nxt;
    int32_t mend = 0;
    
    trie = self->trie;
    while (mend < len) {
        nxt = ubuf[mend++];
        newtrie = trie->next[nxt];
        if (newtrie == NULL) {
            newtrie = trie->fail;
            mend--;
        }
        if (newtrie == NULL) {
            trie = self->trie;
            mend++;
        } else if (newtrie->outs != NULL) {
            match->position = mend - newtrie->depth;
            match->length = newtrie->depth;
            match->whichpattern = newtrie->outs->whichpattern;
            return 1;
        } else {
            trie = newtrie;
        }
    }
    return 0;
}

int32_t FgrepAhoFindAll ( FgrepAhoParams *self,
    char *buf, int32_t len, int32_t *whichpattern )
{
    unsigned char *ubuf = (unsigned char *)buf;
    struct trie *trie;
    struct trie *newtrie;

    unsigned char nxt;
    int32_t mend = 0;
    
    trie = self->trie;
    while (mend < len) {
        nxt = ubuf[mend++];
        newtrie = trie->next[nxt];
        if (newtrie == NULL) {
            newtrie = trie->fail;
            mend--;
        }
        if (newtrie == NULL) {
            trie = self->trie;
            mend++;
        } else if (newtrie->outs != NULL) {
            *whichpattern = newtrie->outs->whichpattern;
            return mend - newtrie->depth;
        } else {
            trie = newtrie;
        }
    }
    return -1;
}
