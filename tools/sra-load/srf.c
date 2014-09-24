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
#include <klib/rc.h>

#include "debug.h"
#include "srf.h"
#include "ztr.h"

#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <stdio.h>
#include <endian.h>
#include <byteswap.h>
#include <assert.h>

enum RCState SRF_ParseChunk(const uint8_t *src, size_t srclen, uint64_t *size, enum SRF_ChunkTypes *type, size_t *data)
{
    assert(size);
    assert(type);
    assert(data);
    
    *size = 0;
    *type = SRF_ChunkTypeUnknown;
    *data = 0;
    
    if(srclen == 0) {
        return rcInsufficient;
    }
    *type = src[0];
    switch(*type) {
        case 'S':
            if (srclen < 8) {
                return rcInsufficient;
            }
            if (*(const uint32_t *)src != *(const uint32_t *)("SSRF")) {
                return rcInvalid;
            }
            *size = (((((src[4] << 8) | src[5]) << 8) | src[6]) << 8) | src[7];
            *data = 8;
            break;
        case 'I':
            if(srclen >= 16) {
                *size = (((((((((((((src[8] << 8) | src[9]) << 8) | src[10]) << 8) | src[11]) << 8) |
                              src[12]) << 8) | src[13]) << 8) | src[14]) << 8) | src[15];
            }
            break;
        case 'X':
        case 'H':
        case 'R':
            if (srclen < 5) {
                return rcInsufficient;
            }
            *size = (((((src[1] << 8) | src[2]) << 8) | src[3]) << 8) | src[4];
            *data = 5;
            break;
        default:
            return rcInvalid;
    }
    return 0;
}

int SRF_ParseContainerHeader(const uint8_t *src, uint64_t src_length,
                             unsigned *versMajor, unsigned *versMinor,
                             char *contType, pstring *baseCaller, pstring *baseCallerVersion)
{
    const uint8_t *vers = src + 1;
    
    src += *src + 1;
    
    if (versMajor != NULL) {
        *versMajor = 0;
        
        while (vers != src && *vers != '.') {
            if (*vers > '9' || *vers < '0')
                return rcInvalid;
            *versMajor = *versMajor * 10 + *vers - '0';
            ++vers;
        }
        if (versMinor != NULL) {
            *versMinor = 0;
            
            if (vers != src && *vers == '.')
                ++vers;
            while (vers != src) {
                if (*vers > '9' || *vers < '0')
                    return rcInvalid;
                *versMinor = *versMinor * 10 + *vers - '0';
                ++vers;
            }
        }
    }
    
    if (contType != NULL)
        *contType = *src;
    ++src;
    
    if (baseCaller != NULL) {
        if( pstring_assign(baseCaller, src, *src + 1) != 0 ) {
            return rcInsufficient;
        }
    }
    src += *src + 1;
    
    if (baseCallerVersion != NULL) {
        if( pstring_assign(baseCallerVersion, src, *src + 1) != 0 ) {
            return rcInsufficient;
        }
    }
    return 0;
}

int SRF_ParseDataChunk(const uint8_t *src, size_t src_length, size_t *parsed_length, pstring *prefix,
                       char *prefixType, uint32_t *counter)
{
    const uint8_t *const endp = src + src_length;
    assert(src);
    assert(parsed_length);
    
    if (counter != NULL)
        *counter = 0;
    if (prefix != NULL)
        pstring_clear(prefix);
    
    if (src_length < 2)
        return rcInsufficient;
    
    if (prefixType != NULL)
        *prefixType = *src;
    ++src;
    if (*src == 0)
        ++src;
    else if (src + *src + 1 > endp)
        return rcInsufficient;
    else {
        if (prefix != NULL) {
            if( pstring_assign(prefix, src + 1, *src) != 0 ) {
                return rcInsufficient;
            }
        }
        src += *src + 1;
    }
#if 1
    switch (endp - src) {
        case 0:
            break;
        case 4:
            if (counter != NULL)
                *counter = (((((src[0] << 8) | src[1]) << 8) | src[2]) << 8) | src[3];
            src += 4;
            break;
        default:
            if (endp - src < 8)
                return rcInsufficient;
            if (memcmp(src, ZTR_SIG, 8) != 0) {
                if (counter != NULL)
                    *counter = (((((src[0] << 8) | src[1]) << 8) | src[2]) << 8) | src[3];
                src += 4;
            }
            break;
    }
#else
    if (!ZTR_IsSignature(src, endp - src)) {
        if (src + 4 > endp)
            return rcInsufficient;
        if (counter != NULL)
            *counter = (((((src[0] << 8) | src[1]) << 8) | src[2]) << 8) | src[3];
        src += 4;
    }
#endif
    *parsed_length = src_length - (endp - src);
    return 0;
}

int SRF_ParseReadChunk(const uint8_t *src, size_t src_length, size_t *parsed_length, uint8_t *flags, pstring *id)
{
    const uint8_t* const endp = src + src_length;
    
    assert(src);
    assert(parsed_length);
    assert(flags);
    assert(id);
    
    *flags = 0;
    pstring_clear(id);

    if(src_length < 2) {
        return rcInsufficient;
    }
    *flags = *src++;
    if(*src == '\0') {
        ++src;
    } else if(src + *src + 1 > endp) {
        return rcInsufficient;
    } else {
        if( pstring_assign(id, src + 1, *src) != 0 ) {
            return rcInsufficient;
        }
        src += *src + 1;
    }
    *parsed_length = src_length - (endp - src);
    if( *flags != 0 ) {
        DEBUG_MSG(5, ("SRF_FLAGS='%02X'\n", (int)(*flags)));
    }
    return 0;
}
