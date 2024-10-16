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

#ifndef _h_search_priv_
#define _h_search_priv_

#include <search/grep.h>
#include <klib/rc.h>
#include <os-native.h>
#include <compiler.h>
#include <insdc/insdc.h>

#include <stdlib.h>
#include <string.h>

#include <arch-impl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FgrepDumbParams FgrepDumbParams;
typedef struct FgrepBoyerParams FgrepBoyerParams;
typedef struct FgrepAhoParams FgrepAhoParams;
typedef struct AgrepWuParams AgrepWuParams;
typedef struct DPParams DPParams;
typedef struct MyersSearch MyersSearch;
typedef struct MyersUnlimitedSearch MyersUnlimitedSearch;

void FgrepDumbSearchMake(FgrepDumbParams **self, const char *strings[], uint32_t numstrings);
void FgrepDumbSearchFree(FgrepDumbParams *self);

void FgrepBoyerSearchMake(FgrepBoyerParams **self, const char *strings[], uint32_t numstrings);
void FgrepBoyerSearchFree(FgrepBoyerParams *self);

void FgrepAhoMake(FgrepAhoParams **self, const char *strings[], uint32_t numstrings);
void FgrepAhoFree(FgrepAhoParams *self);

uint32_t FgrepDumbFindFirst(FgrepDumbParams *self, const char *buf, size_t len, FgrepMatch *match);
uint32_t FgrepBoyerFindFirst( FgrepBoyerParams *self, const char *buf, size_t len, FgrepMatch *match);
uint32_t FgrepAhoFindFirst( FgrepAhoParams *self, const char *buf, size_t len, FgrepMatch *match);

void AgrepWuFree(AgrepWuParams *self);
void MyersUnlimitedFree(MyersUnlimitedSearch *self);
void AgrepDPFree(DPParams *self);
void AgrepMyersFree(MyersSearch *self);

uint32_t MyersUnlimitedFindFirst(MyersUnlimitedSearch *self, int32_t threshold, const char* text, size_t n, AgrepMatch *match);
uint32_t MyersFindFirst(MyersSearch *self, int32_t threshold, 
                   const char* text, size_t n,
                       AgrepMatch *match);
uint32_t AgrepWuFindFirst(const AgrepWuParams *self, int32_t threshold, const char *buf, int32_t buflen, AgrepMatch *match);
uint32_t AgrepDPFindFirst(const DPParams *self, int32_t threshold, AgrepFlags mode, const char *buf, int32_t buflen, AgrepMatch *match);

void AgrepDPFindAll(const AgrepCallArgs *args);
void MyersFindAll(const AgrepCallArgs *args);
void MyersUnlimitedFindAll(const AgrepCallArgs *args);
void AgrepWuFindAll(const AgrepCallArgs *args);
int32_t FgrepAhoFindAll(FgrepAhoParams *self, char *buf, int32_t len, int32_t *whichpattern);
void FgrepBoyerFindAll(FgrepBoyerParams *self, char *buf, int32_t len, FgrepMatchCallback cb, void *cbinfo);
void FgrepDumbFindAll(FgrepDumbParams *self, char *buf, int32_t len, 
                      FgrepMatchCallback cb, void *cbinfo);


rc_t AgrepDPMake(DPParams **self, AgrepFlags mode, const char *pattern);
rc_t AgrepMyersMake(MyersSearch **self, AgrepFlags mode, const char *pattern);
rc_t MyersUnlimitedMake(MyersUnlimitedSearch **self, AgrepFlags mode, const char *pattern);
rc_t AgrepWuMake(AgrepWuParams **self, AgrepFlags mode, const char *pattern);


struct Fgrep {
    struct FgrepDumbParams *dumb;
    struct FgrepBoyerParams *boyer;
    struct FgrepAhoParams *aho;
    FgrepFlags mode;
};

typedef struct Fgrep FgrepParams;

/* 
We actually need to have multiple (non-union) for Myers and DP parameters,
since the myers code uses the DP parameters.
Some of the others could be unioned.
*/
struct Agrep
{
    struct AgrepWuParams *wu;
    struct MyersSearch *myers;
    struct MyersUnlimitedSearch *myersunltd;
    struct DPParams *dp;
    AgrepFlags mode;
};

typedef struct Agrep AgrepParams;

extern const unsigned char* IUPAC_decode[256];
rc_t na4_set_bits(const AgrepFlags mode, uint64_t* arr, const unsigned char c, const uint64_t val);
void set_bits_2na(uint64_t* arr, unsigned char c, uint64_t val);

/* Internal definitions */

rc_t CC dp_end_callback( const void *cbinfo, const AgrepMatch *match, AgrepContinueFlag *flag );

static __inline__
char *create_substring ( const char *src, uint32_t sz )
{
    char *ret = (char*)malloc( sz + 1 );
    strncpy ( ret, src, sz );
    ret [ sz ] = 0;
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* _h_search_priv_ */
