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
#include <klib/log.h>
#include <klib/rc.h>
#include <sra/wsradb.h>
#include <sra/types.h>

#include "common-xml.h"
#include "pstring.h"
#include "debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

rc_t pstring_clear(pstring* str)
{
    if( str == NULL ) {
        return RC(rcSRA, rcFormatter, rcCopying, rcParam, rcNull);
    }
    str->data[0] = '\0';
    str->len = 0;
    return 0;
}


rc_t pstring_copy(pstring* dst, const pstring* src)
{
    if( src == NULL ) {
        return RC(rcSRA, rcFormatter, rcCopying, rcParam, rcNull);
    }
    return pstring_assign(dst, src->data, src->len);
}

rc_t pstring_assign(pstring* str, const void* data, const size_t data_sz)
{
    if( str == NULL || data == NULL ) {
        return RC(rcSRA, rcFormatter, rcCopying, rcParam, rcNull);
    } else if( data_sz > sizeof(str->data) - 1 ) {
        return RC(rcSRA, rcFormatter, rcCopying, rcBuffer, rcInsufficient);
    }
    memmove(str->data, data, data_sz);
    str->len = data_sz;
    str->data[str->len] = '\0';
    return 0;
}

rc_t pstring_concat(pstring* dst, const pstring* src)
{
    return pstring_append(dst, src->data, src->len);
}

rc_t pstring_append(pstring* str, const void* data, const size_t data_sz)
{
    if( str == NULL || data == NULL ) {
        return RC(rcSRA, rcFormatter, rcCopying, rcParam, rcNull);
    } else if( str->len + data_sz > sizeof(str->data) - 1 ) {
        return RC(rcSRA, rcFormatter, rcCopying, rcBuffer, rcInsufficient);
    }
    memmove(&str->data[str->len], data, data_sz);
    str->len += data_sz;
    str->data[str->len] = '\0';
    return 0;
}

rc_t pstring_append_chr(pstring* str, char c, size_t count)
{
    if( str == NULL ) {
        return RC(rcSRA, rcFormatter, rcCopying, rcParam, rcNull);
    } else if( str->len + count > sizeof(str->data) - 1 ) {
        return RC(rcSRA, rcFormatter, rcCopying, rcBuffer, rcInsufficient);
    }
    memset(&str->data[str->len], c, count);
    str->len += count;
    str->data[str->len] = '\0';
    return 0;
}

int pstring_cmp(const pstring* str1, const pstring* str2)
{
    int r = str1->len - str2->len;
    return r ? r : memcmp(str1->data, str2->data, str1->len);
}

int pstring_strcmp(const pstring* str1, const char* str2)
{
    int r = str1->len - strlen(str2);
    return r ? r : memcmp(str1->data, str2, str1->len);
}

bool pstring_is_fasta(const pstring* str)
{
    if( str != NULL ) {
        const char* s = str->data;
        const char* end = s + str->len;
        while( s != end ) {
            if( strchr("ACGTNactgn.", *s++) == NULL ) {
                return false;
            }
        }
        return str->len != 0;
    }
    return false;
}

bool pstring_is_csfasta(const pstring* str)
{
    if( str != NULL ) {
        const char* s = str->data;
        const char* end = s + str->len;

        if( s == end || strchr("ACGTacgt", *s++) == NULL ) {
            return false;
        }
        while( s != end ) {
            if( strchr("0123.", *s++) == NULL ) {
                return false;
            }
        }
        return true;
    }
    return false;
}

rc_t pstring_quality_convert(pstring* qstr, ExperimentQualityEncoding enc, const uint8_t offset, const int8_t min, const int8_t max)
{
    rc_t rc = 0;
    char* c, *end, *next;
    pstring qbin;

    if( qstr == NULL || min > max ) {
        rc = RC(rcSRA, rcFormatter, rcReading, rcParam, rcInvalid);
    }
    errno = 0;
    c = qstr->data;
    end = qstr->data + qstr->len;
    pstring_clear(&qbin);
    if(enc == eExperimentQualityEncoding_Undefined) {
	if(memchr(c, ' ', qstr->len) != NULL || memchr(c, '\t', qstr->len) != NULL){
		enc = eExperimentQualityEncoding_Decimal;
	} else {
		enc = eExperimentQualityEncoding_Ascii;
	}
    }


    while( rc == 0 && c < end ) {
        long q;
        switch(enc) {
            case eExperimentQualityEncoding_Decimal:
            case eExperimentQualityEncoding_Hexadecimal:
                /* spaced numbers form */
                errno = 0;
                q = strtol(c, &next, enc == eExperimentQualityEncoding_Decimal ? 10 : 16);
                if( q == 0 && c == next ) {
                    /* no more digits in line */
		    goto DONE; /*** need do break while loop as well ***/
                }
                c = next;
                break;
            case eExperimentQualityEncoding_Ascii:
                /* textual form with offset */
                q = (long)(*c++) - offset;
                break;
            default:
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcUnrecognized);
                break;
        }
        if( rc == 0 ) {
            if( errno != 0 || q < min || q > max ) {
                rc = RC(rcSRA, rcFormatter, rcReading, rcData, rcOutofrange);
            } else {
                rc = pstring_append_chr(&qbin, (int8_t)q, 1);
            }
        }
    }
DONE:
    if( rc == 0 ) {
        rc = pstring_copy(qstr, &qbin);
    }
    return rc;
}
