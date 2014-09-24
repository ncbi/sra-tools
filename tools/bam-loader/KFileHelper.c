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

#include "KFileHelper.h"
#include <string.h>

rc_t BufferedKFileGets(KDataBuffer *buf, unsigned offset, unsigned *nread, BufferedKFile *strm) {
    unsigned i;
    unsigned n;
    int ch;
    rc_t rc;
    
    do {
        if (strm->cur != strm->inbuf) {
            ch = strm->buf[strm->cur];
            
            if (ch == '\r' || ch == '\n')
                ++strm->cur;
            else
                break;
        }
        else {
            size_t numread;
            
            rc = KFileRead(strm->kfp, strm->fpos, strm->buf, sizeof(strm->buf), &numread);
            if (rc) return -1;
            
            strm->cur = 0;
            strm->inbuf = numread;
            strm->fpos += numread;
            if (numread == 0) {
                *nread = 0;
                return 0;
            }
        }
    } while (1);
    
    for (n = 0, i = offset; ; ++i, ++n) {
        if (strm->cur != strm->inbuf) {
            ch = strm->buf[strm->cur++];
        }
        else {
            size_t numread;
            
            rc = KFileRead(strm->kfp, strm->fpos, strm->buf, sizeof(strm->buf), &numread);
            if (rc) return rc;
            
            strm->cur = 0;
            strm->inbuf = numread;
            if (numread == 0) break;
            strm->fpos += numread;
            ch = strm->buf[strm->cur++];
        }
        if (ch == '\r' || ch == '\n') break;
        if (i >= buf->elem_count) {
            rc = KDataBufferResize(buf, buf->elem_count ? buf->elem_count * 2 : 1024);
            if (rc) return rc;
        }
        ((char *)buf->base)[i] = ch;
    }
    *nread = n;
    return 0;
}

rc_t BufferedKFileOpen(const KDirectory *directory, BufferedKFile *rslt, const char *fileName, ...) {
    va_list va;
    rc_t rc;

    memset(rslt, 0, sizeof(rslt) - sizeof(rslt->buf));

    va_start(va, fileName);
    rc = KDirectoryVOpenFileRead(directory, &rslt->kfp, fileName, va);
    va_end(va);
    
    return rc;
}

rc_t BufferedKFileClose(BufferedKFile *strm) {
    return KFileRelease(strm->kfp);
}

rc_t LoadFile(KDataBuffer *dst, uint64_t *lineCount, const KDirectory *dir, const char *fileName, ...) {
    BufferedKFile strm;
    rc_t rc;
    unsigned line = 0;

    memset(&strm, 0, sizeof(strm) - sizeof(strm.buf));
    {
        va_list va;

        va_start(va, fileName);
        rc = KDirectoryVOpenFileRead(dir, &strm.kfp, fileName, va);
        va_end(va);
    }
    if (rc == 0) {
        unsigned offset = 0;
        unsigned i;
        
        while ((rc = BufferedKFileGets(dst, offset, &i, &strm)) == 0 && i) {
            offset += i;
            if (offset + 1 >= dst->elem_count) {
                rc = KDataBufferResize(dst, offset + 1);
                if (rc)
                    break;
            }
            ((char *)dst->base)[offset++] = '\0';
            ++line;
        }
        BufferedKFileClose(&strm);
        if (rc == 0)
            rc = KDataBufferResize(dst, offset);
    }
    if (rc != 0) {
        KDataBufferResize(dst, 0);
        line = 0;
    }
    if (lineCount) *lineCount = line;
    return rc;
}
