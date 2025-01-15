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
#define index strchr
#include <search/extern.h>
#include <sysalloc.h>
#include "../../libs/search/search-priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
/*#include <unistd.h>*/
#include <stdio.h>
#include <stdlib.h>

#include <vdb/manager.h> /* VDBManager */
#include <vdb/table.h> /* VTable */
#include <vdb/cursor.h> /* VCursor */


#ifndef min
#define min(a, b) ((b) < (a) ? (b) : (a))
#endif


typedef struct AgrepBestMatch
{
    int32_t nScore;
    int32_t indexStart;
    int32_t nMatchLength;
} AgrepBestMatch;

typedef struct agrepinfo {
    char const* buf;
    int32_t linenum;
    size_t original_length;
    AgrepBestMatch* pBestMatch;
} agrepinfo;


rc_t agrep_callback_find_best( void const* cbinfo_void, AgrepMatch const* match, AgrepContinueFlag *cont )
{
    agrepinfo const* cbinfo = (agrepinfo *)cbinfo_void;
    *cont = AGREP_CONTINUE;
    if (cbinfo->pBestMatch->nScore > match->score)
    {
        cbinfo->pBestMatch->nScore = match->score;
        cbinfo->pBestMatch->indexStart = match->position;
        cbinfo->pBestMatch->nMatchLength = match->length;
    }

    return 0;
}

rc_t agrep_callback( void const* cbinfo_void, AgrepMatch const* match, AgrepContinueFlag *cont )
{
    agrepinfo const* cbinfo = (agrepinfo *)cbinfo_void;
    int32_t const nLine = cbinfo->linenum;
    int32_t const indexStart = match->position;
    int32_t const nMatchLength = match->length;
    size_t const nOriginalLen = cbinfo->original_length;
    char const* pszBuf = cbinfo->buf;
    int const bIsCutInBetween = (indexStart + nMatchLength) > nOriginalLen;
    *cont = AGREP_CONTINUE;

    printf("line=%d, pos=%d, len=%d, end=%d, score=%d: %.*s%s%.*s\n",
        nLine, indexStart, nMatchLength,
        (indexStart + nMatchLength) % (int32_t)nOriginalLen,
        match->score,
        (int)(min(nMatchLength, nOriginalLen - indexStart)), pszBuf + indexStart,
        bIsCutInBetween ? "|" : "",
        (int)(bIsCutInBetween ? nMatchLength - (nOriginalLen - indexStart) : 0), pszBuf);
    return 0;
}


rc_t agrep_2na_callback( void const* cbinfo, AgrepMatch const* match, AgrepContinueFlag *cont )
{
    char const* p;
    static char *buf = NULL;
    static int32_t buflen = 0;
    int32_t i;

    *cont = AGREP_CONTINUE;
    if (buf == NULL || buflen < match->length) {
        free(buf);
        buf = malloc(match->length+1);
        buflen = match->length;
    }
    p = ((agrepinfo const *)cbinfo)->buf + match->position;
    for (i=0; i<match->length; i++) {
        buf[i] = "ACGTN"[(size_t)p[i]];
    }
    buf[match->length] = '\0';

    /* TODO: this probably requires the same processing as in agrep_callback */
    printf("%d,%d,%d: %s\n", ((agrepinfo const *)cbinfo)->linenum, match->position, match->score, buf);
    return 0;
}


#define BUF_SIZE 4096

LIB_EXPORT int32_t CC agrep ( AgrepFlags alg, char const* pattern, FILE* fp, int32_t print, 
        int32_t printcontext, int32_t k, int32_t findall, int32_t simulate )
{
    AgrepParams *self = NULL;
    int32_t count;
    size_t len;
    AgrepFlags mode = alg;
    AgrepMatch match;
    agrepinfo info;
    char szBuf[BUF_SIZE];

    AgrepCallArgs args;

    int32_t linenum = 0;
    char const* na = "ACGTN";
    size_t const nPatternSize = strlen(pattern);
    size_t const nBufSize = BUF_SIZE;
    size_t const nMaxTextLen = nBufSize-nPatternSize-1;
    size_t nExtendBufSize;

    int32_t i;

    AgrepMake(&self, mode, pattern);

    count = 0;

    while (!feof(fp) && fgets(szBuf, nMaxTextLen, fp))
    {
        ++linenum;
        len = strlen(szBuf);
        if (szBuf[len-1] == '\n')
        {
            szBuf[len-1] = '\0';
            --len;
        }

        /* Make szBuf somewhat circular to be able to find pattern cut in between */
        nExtendBufSize = nPatternSize <= len ? nPatternSize : len;
        memmove(szBuf+len, szBuf, nExtendBufSize - 1);
        szBuf[len + nExtendBufSize - 1] = '\0';

        if (simulate)
        {
            for (i = 0; szBuf[i]; ++i)
            {
                if (NULL != index(na, szBuf[i]))
                {
                    szBuf[i] = index(na, szBuf[i]) - na;
                }
            }
        }
        
        if (findall)
        {
            AgrepBestMatch best_match = {9999, 0, 0}; /* TODO: it's not C89*/

            info.buf = szBuf;
            info.linenum = linenum;
            info.original_length = len;
            info.pBestMatch = &best_match;

            args.self = self;
            args.threshold = k;
            args.buf = szBuf;
            args.buflen = len + nPatternSize;
            args.cb = simulate ? agrep_2na_callback : agrep_callback_find_best;
            args.cbinfo = (void *)&info;

            AgrepFindAll(&args);

            if (!simulate) /* output best match */
            {
                printf("line=%d, pos=%d, len=%d, end=%d, score=%d: %.*s%s%.*s\n",
                    linenum, best_match.indexStart, best_match.nMatchLength,
                    (best_match.indexStart + best_match.nMatchLength) % (int32_t)len,
                    best_match.nScore,
                    (int)(min(best_match.nMatchLength, (int32_t)len - best_match.indexStart)), szBuf + best_match.indexStart,
                    (best_match.indexStart + best_match.nMatchLength) > (int32_t)len ? "|" : "",
                    (int)((best_match.indexStart + best_match.nMatchLength) > (int32_t)len ? best_match.nMatchLength - ((int32_t)len - best_match.indexStart) : 0), szBuf);
            }
        }
        else if (AgrepFindFirst(self, k, szBuf, len + nPatternSize, &match))
        {
            ++count;
            if (print)
            {
                printf("%s\n", szBuf);
            }
            if (printcontext)
            {
                printf("%.*s\n", match.length, szBuf + match.position);
            }
        }
    }

    return count;
}

int main_agrep_test( int ac, char const** av )
{
    char const* file;
    FILE *fp;
    int32_t count;
    int32_t slow = 0;
    char const* pattern;
    int32_t print = 1;
    int32_t printcontext = 0;
    char *digits = "0123456789";
    int32_t k=0;
    AgrepFlags alg = AGREP_ALG_WUMANBER | AGREP_EXTEND_BETTER | AGREP_LEFT_MAINTAIN_SCORE;
    AgrepFlags mode = 0;
    int32_t findall = 0;
    int32_t simulate = 0;

    while (ac > 2 && av[1][0] == '-') {
        if (!strcmp("--4na", av[1])) {
            mode |= AGREP_PATTERN_4NA;
            ac--; av++;
            continue;
        }
        if (!strcmp("--extend", av[1])) {
            mode |= AGREP_EXTEND_SAME;
            ac--; av++;
            continue;
        }
        if (!strcmp("--better", av[1])) {
            mode |= AGREP_EXTEND_BETTER;
            ac--; av++;
            continue;
        }
        if (!strcmp("--maintain", av[1])) {
            mode |= AGREP_LEFT_MAINTAIN_SCORE;
            ac--; av++;
            continue;
        }

        if (!strcmp("--anchor", av[1])) {
            mode |= AGREP_ANCHOR_LEFT;
            ac--; av++;
            continue;
        }
        
        if (!strcmp("--2na", av[1])) {
            simulate = 1;
            mode |= AGREP_TEXT_EXPANDED_2NA | AGREP_PATTERN_4NA;
            ac--; av++;
            continue;
        }
        if (!strcmp("-a", av[1])) {
            findall = 1;
            printcontext = 1;
            ac--; av++;
            continue;
        }
        if (NULL != index(digits, av[1][1])) {
            k = atoi(++av[1]);
            ac--; av++;
            continue;
        }
        if (!strcmp(av[1], "--myers")) {
            alg = AGREP_ALG_MYERS;
            ac--; av++;
            continue;
        }
        if (!strcmp(av[1], "--dp")) {
            alg = AGREP_ALG_DP;
            ac--; av++;
            continue;
        }
        if (!strcmp(av[1], "--myersunlimited")) {
            alg = AGREP_ALG_MYERS_UNLTD;
            ac--; av++;
            continue;
        }
        if (!strcmp(av[1], "--myersunltd")) {
            alg = AGREP_ALG_MYERS_UNLTD;
            ac--; av++;
            continue;
        }
        if (!strcmp(av[1], "--wu")) {
            alg = AGREP_ALG_WUMANBER;
            ac--; av++;
            continue;
        }
        if (!strcmp(av[1], "--wumanber")) {
            alg = AGREP_ALG_WUMANBER;
            ac--; av++;
            continue;
        }

        if (!strcmp(av[1], "--slow")) {
            slow = 1;
            ac--; av++;
            continue;
        }
        if (!strcmp(av[1], "-c")) {
            print = 0;
            ac--; av++;
            continue;
        }
        if (!strcmp(av[1], "-x")) {
            printcontext = 1;
            print = 0;
            ac--; av++;
            continue;
        }
        fprintf(stderr, "Unknown switch: %s\n", av[1]);
        ac--; av++;
    }

    pattern = av[1];
    file = av[2];
  
    mode |= AGREP_MODE_ASCII; /* since we open file in text mode, force AGREP_MODE_ASCII */
    fp = fopen(file, "r");
    count = agrep(alg|mode, pattern, fp, print, printcontext, k, findall, simulate);

    fclose(fp);

    return 0;
}
