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

#include "debug.h"
#include "zlib-simple.h"

rc_t ZLib_DeflateBlock(const char* src, size_t src_sz, char* dst, size_t dst_sz, size_t* written)
{
    rc_t rc = 0;
    z_stream z_strm;
    int z_err = Z_OK;

    if( src == NULL || dst == NULL || written == NULL ) {
        rc = RC(rcExe, rcFunction, rcConstructing, rcParam, rcNull);
    } else {
        z_strm.next_in = (unsigned char*)src;
        z_strm.zalloc = Z_NULL;
        z_strm.zfree = Z_NULL;
        z_strm.opaque = Z_NULL;
        z_err = deflateInit2(&z_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
        if( z_err != Z_OK ) {
            rc = RC(rcExe, rcFunction, rcConstructing, rcInterface, rcUnexpected);
            DEBUG_MSG(3, ("deflateInit2: %R %d\n", rc, z_err));
        } else {
            z_strm.avail_in = src_sz;
            z_strm.next_out = (unsigned char*)dst;
            z_strm.avail_out = dst_sz;
            if( (z_err = deflate(&z_strm, Z_FINISH)) != Z_STREAM_END ) {
                rc = RC(rcExe, rcFunction, rcExecuting, rcInterface, rcUnexpected);
                DEBUG_MSG(3, ("deflate(Z_FINISH): %R %d\n", rc, z_err));
            }
            *written = dst_sz - z_strm.avail_out;
            if( (z_err = deflateEnd(&z_strm)) != Z_OK ) {
                rc = RC(rcExe, rcFunction, rcDestroying, rcInterface, rcUnexpected);
                DEBUG_MSG(3, ("deflateEnd: %r %d\n", rc, z_err));
            }
        }
    }
    return rc;
}
