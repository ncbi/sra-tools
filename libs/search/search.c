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
#include <sysalloc.h>
#include "search-priv.h"

#include <stdlib.h>
#include <string.h>

const unsigned char * IUPAC_decode [ 256 ];

static
void IUPAC_init ( void )
{
    static bool initialized;
    if ( ! initialized )
    {
        const char ** t = ( const char** ) IUPAC_decode;

        t['A'] = t['a'] = "Aa";
        t['C'] = t['c'] = "Cc";
        t['G'] = t['g'] = "Gg";
        t['T'] = t['t'] = "Tt";
        t['U'] = t['u'] = "Uu";
        t['M'] = t['m'] = "AaCc";
        t['R'] = t['r'] = "AaGg";
        t['S'] = t['s'] = "CcGg";
        t['V'] = t['v'] = "AaCcGg";
        t['W'] = t['w'] = "AaTtUu";
        t['Y'] = t['y'] = "CcTtUu";
        t['K'] = t['k'] = "GgTtUu";
        t['B'] = t['b'] = "CcGgTtUu";
        t['D'] = t['d'] = "AaGgTtUu";
        t['H'] = t['h'] = "AaCcTtUu";
        t['N'] = t['n'] = t [ '.' ] = "AaCcGgTtUuNn.-";
        initialized = true;
    }
}

void set_bits_2na(uint64_t* arr, unsigned char c, uint64_t val)
{
    unsigned char const* tr;
    for (tr = IUPAC_decode[c]; *tr != '\0'; ++tr)
    {
        switch(*tr)
        {
        case 'A':
            arr[0] |= val;
            break;
        case 'C':
            arr[1] |= val;
            break;
        case 'G':
            arr[2] |= val;
            break;
        case 'T':
        /*case 'U':*/
            arr[3] |= val;
            break;
        /*case 'N':
            arr[4] |= val;
            break;*/
        }
    }
}

rc_t na4_set_bits(const AgrepFlags mode, uint64_t* arr, const unsigned char c, const uint64_t val)
{
    if( mode & AGREP_PATTERN_4NA ) {
        const unsigned char* tr;
        tr = IUPAC_decode[c];
        if( tr == NULL ) {
            if( (mode & AGREP_ANYTHING_ELSE_IS_N) && (c == '.' || c == '-') ) {
                tr = IUPAC_decode['N'];
            }
            if( tr == NULL ) {
                return RC(rcText, rcString, rcSearching, rcConstraint, rcOutofrange);
            }
        }
        while( *tr != '\0' ) {
            if( mode & AGREP_TEXT_EXPANDED_2NA ) {
                switch(*tr) {
                    case 'A':
                        arr[0] |= val;
                        break;
                    case 'C':
                        arr[1] |= val;
                        break;
                    case 'G':
                        arr[2] |= val;
                        break;
                    case 'T':
                    case 'U':
                        arr[3] |= val;
                        break;
                    case 'N':
                        arr[4] |= val;
                        break;
                }
            }
            arr[*tr++] |= val;
        }
    }
    return 0;
}

LIB_EXPORT void CC FgrepFree( FgrepParams *self )
{
    if (self->dumb != NULL)
        FgrepDumbSearchFree(self->dumb);
    if (self->boyer != NULL)
        FgrepBoyerSearchFree(self->boyer);
    if (self->aho != NULL)
        FgrepAhoFree(self->aho);
    free(self);
}


LIB_EXPORT rc_t CC FgrepMake( FgrepParams **self, FgrepFlags mode, const char *strings[], uint32_t numstrings )
{
    *self = malloc(sizeof(FgrepParams));
    memset(*self, 0, sizeof(FgrepParams));
    (*self)->mode = mode;
    if (mode & FGREP_ALG_DUMB) {
        FgrepDumbSearchMake(&(*self)->dumb, strings, numstrings);
    }
    if (mode & FGREP_ALG_BOYERMOORE) {
        FgrepBoyerSearchMake(&(*self)->boyer, strings, numstrings);
    }
    if (mode & FGREP_ALG_AHOCORASICK) {
        FgrepAhoMake(&(*self)->aho, strings, numstrings);
    }
    return 0;
}

LIB_EXPORT uint32_t CC FgrepFindFirst( const FgrepParams *self, const char *buf, size_t len, FgrepMatch *match )
{
    if (self->mode & FGREP_ALG_DUMB) {
        return FgrepDumbFindFirst(self->dumb, buf, len, match);
    }
    if (self->mode & FGREP_ALG_BOYERMOORE) {
        return FgrepBoyerFindFirst(self->boyer, buf, len, match);
    }
    if (self->mode & FGREP_ALG_AHOCORASICK) {
        return FgrepAhoFindFirst(self->aho, buf, len, match);
    }
    /* Should maybe return error code, not 1/0 */
    return 0;
}

LIB_EXPORT rc_t CC AgrepMake( AgrepParams **self, AgrepFlags mode, const char *pattern )
{
    rc_t rc = 0;

    if( self == NULL || pattern == NULL ) {
        rc = RC(rcText, rcString, rcSearching, rcParam, rcNull);
    } else if( (*self = malloc(sizeof(AgrepParams))) == NULL ) {
        rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
    } else {
        memset((*self), 0, sizeof(**self));
        (*self)->mode = mode;
        if( mode & AGREP_PATTERN_4NA ) {
            size_t i, l = strlen(pattern);
            IUPAC_init();
            if( l == 0 ) {
                rc = RC(rcText, rcString, rcSearching, rcParam, rcOutofrange);
            }
            for(i = 0; rc == 0 && i < l; i++) {
                if( IUPAC_decode[(signed char)(pattern[i])] == NULL ) {
                    rc = RC(rcText, rcString, rcSearching, rcParam, rcOutofrange);
                }
            }
        } else if( !(mode & AGREP_MODE_ASCII) ) {
            rc = RC(rcText, rcString, rcSearching, rcParam, rcUnsupported);
        }
        if( rc == 0 ) {
            IUPAC_init(); /* TODO: this is temporary solution */
            if(mode & AGREP_ALG_WUMANBER) {
                if( (rc = AgrepWuMake(&(*self)->wu, mode, pattern)) == 0 ) {
                    rc = AgrepDPMake(&(*self)->dp, mode, pattern);
                }
            } else if(mode & AGREP_ALG_MYERS) {
                if( (rc = AgrepMyersMake(&(*self)->myers, mode, pattern)) == 0 ) {
                    /*TODO: now agrep is being used without DP, so we need to turn off AgrepDPMake here*/
                    rc = AgrepDPMake(&(*self)->dp, mode, pattern);
                }
            } else if(mode & AGREP_ALG_MYERS_UNLTD) {
                if( (rc = MyersUnlimitedMake(&(*self)->myersunltd, mode, pattern)) == 0 ) {
                    rc = AgrepDPMake(&(*self)->dp, mode, pattern);
                }
            } else if(mode & AGREP_ALG_DP) {
                rc = AgrepDPMake(&(*self)->dp, mode, pattern);
            } else {
                rc = RC(rcText, rcString, rcSearching, rcParam, rcInvalid);
            }
        }
    }
    if( rc != 0 ) {
        AgrepWhack(*self);
    }
    return rc;
}

LIB_EXPORT void CC AgrepWhack( AgrepParams *self )
{
    if( self != NULL ) {
        if( self->wu ) {
            AgrepWuFree(self->wu);
        }
        if( self->myers ) {
            AgrepMyersFree(self->myers);
        }
        if( self->myersunltd ) {
            MyersUnlimitedFree(self->myersunltd);
        }
        if( self->dp ) {
            AgrepDPFree(self->dp);
        }
        free(self);
    }
}

LIB_EXPORT void CC AgrepFindAll( const AgrepCallArgs *args )
{
    if( args != NULL ) {
        const AgrepParams *self = args->self;

        if(self->mode & AGREP_ALG_WUMANBER) {
            AgrepWuFindAll(args);
        } else if(self->mode & AGREP_ALG_MYERS) {
            MyersFindAll(args);
        } else if(self->mode & AGREP_ALG_MYERS_UNLTD) {
            MyersUnlimitedFindAll(args);
        } else if (self->mode & AGREP_ALG_DP) {
            AgrepDPFindAll(args);
        }
    }
}

LIB_EXPORT uint32_t CC AgrepFindFirst( const AgrepParams *self, int32_t threshold, const char *buf, size_t len, AgrepMatch *match )
{
    if( self != NULL && buf != NULL && match != NULL ) {
        if (self->mode & AGREP_ALG_WUMANBER) {
            return AgrepWuFindFirst(self->wu, threshold, buf, (int32_t)len, match);
        }
        if (self->mode & AGREP_ALG_MYERS) {
            return MyersFindFirst(self->myers, threshold, buf, len, match);
        }
        if (self->mode & AGREP_ALG_MYERS_UNLTD) {
            return MyersUnlimitedFindFirst(self->myersunltd, threshold, buf, len, match);
        }
        if (self->mode & AGREP_ALG_DP) {
            return AgrepDPFindFirst(self->dp, threshold, self->mode, buf, (int32_t)len, match);
        }
    }
    /* Not sure this is the right thing to return. */
    return 0;
}

static 
rc_t CC AgrepFindBestCallback(const void *cbinfo, const AgrepMatch *matchinfo, AgrepContinueFlag *flag)
{
    AgrepMatch *best = (AgrepMatch *)cbinfo;
    if (best->score == -1 || best->score > matchinfo->score) {
        *best = *matchinfo;
    }
    return 0;
}

LIB_EXPORT uint32_t CC AgrepFindBest( const AgrepParams *self, int32_t threshold, const char *buf, int32_t len, AgrepMatch *match )
{
    if( self != NULL && buf != NULL && match != NULL ) {
        AgrepCallArgs args;

        args.self = self;
        args.threshold = threshold;
        args.buf = buf;
        args.buflen = len;
        args.cb = AgrepFindBestCallback;
        args.cbinfo = match;

        match->score = -1;
        
        AgrepFindAll(&args);
        if (match->score != -1) {
            return 1;
        }
    }
    return 0;
}


LIB_EXPORT size_t CC FindLongestCommonSubstring(char const* pS1, char const* pS2, size_t const nLen1, size_t const nLen2, size_t* pRetStart1, size_t* pRetStart2)
{
/*
    return: the length of the substring found
    pRetStart1: ptr to return value - starting position on the substring found in the pS1, NULL is OK
    pRetStart2: ptr to return value - starting position on the substring found in the pS2, NULL is OK

    WARNING: the memory usage is optimized to store only one row (not column) of DP matrix
    and there is hardcoded limit for the number of columns
    so it's recommended to pass the shortest string as pS2 parameter.
    Intended usage: pS1 - text (read), pS2 - pattern (linker, bar-code)

    TODO: use suffix arrays + LCP to achieve O(n) complexity (n = nLen1 + nLen2)
*/

    size_t dpPrevRow[64]; /* this is row #(iRow-1) of DP matrix on each step of iRow-loop below*/

    size_t dpCurNewCell;  /* this is cell (iRow, iCol) of DP matrix on each step inside iCol-loop below */
    size_t dpPrevNewCell; /* this is cell (iRow, iCol-1) of DP matrix on each step inside iCol-loop below*/
    size_t iRow, iCol;
    size_t start_placeholder;
    size_t nMaxLen = 0;

    dpPrevNewCell = 0;
    dpCurNewCell = 0;

    assert(sizeof(dpPrevRow)/sizeof(dpPrevRow[0]) >= nLen2);

    /* allow NULLs in pRetStart1 or pRetStart2 */
    if (!pRetStart1)
        pRetStart1 = &start_placeholder;
    if (!pRetStart2)
        pRetStart2 = &start_placeholder;

    *pRetStart1 = *pRetStart2 = 0;

    for (iRow = 0; iRow < nLen1; ++iRow)
    {
        for (iCol = 0; iCol < nLen2; ++iCol)
        {
            if (pS1[iRow] == pS2[iCol])
            {
                if (iRow == 0 || iCol == 0)
                {
                    dpPrevRow[iCol] = 1;
                    dpCurNewCell = 1;
                }
                else
                    dpCurNewCell = dpPrevRow[iCol - 1] + 1;

                if (dpCurNewCell > nMaxLen)
                {
                    nMaxLen = dpCurNewCell;
                    *pRetStart1 = iRow; /* now it's actually end. It's to be corrected before return */
                    *pRetStart2 = iCol; /* now it's actually end. It's to be corrected before return */
                }
            }
            else
            {
                dpCurNewCell = 0;
            }

            if (iCol > 0)
                dpPrevRow[iCol - 1] = dpPrevNewCell;
            dpPrevNewCell = dpCurNewCell;
        }
        dpPrevRow[iCol - 1] = dpCurNewCell;
    }
    *pRetStart1 = *pRetStart1 - nMaxLen + 1;
    *pRetStart2 = *pRetStart2 - nMaxLen + 1;
    return nMaxLen;
}
