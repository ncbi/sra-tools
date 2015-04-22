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

#ifndef _h_utf8_like_int_codec_
#define _h_utf8_like_int_codec_

#include <stdint.h>
#include <stdlib.h> /* size_t on linux */

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    CODEC_INSUFFICIENT_BUFFER = 0,
    CODEC_INVALID_FORMAT = -1,
    CODEC_UNKNOWN_ERROR = -2
};

/*
all encode_uintXX return:
    value <= 0: error:   one of CODEC_* above
    value > 0:  success: number of bytes written to buf_start

all decode_uintXX return:
    value <= 0: error:   one of CODEC_* above
    value > 0:  success: number of bytes read from buf_start
*/

int encode_uint16 ( uint16_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend );
int decode_uint16 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint16_t* ret_decoded );

int encode_uint32 ( uint32_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend );
int decode_uint32 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint32_t* ret_decoded );

int encode_uint64 ( uint64_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend );
int decode_uint64 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint64_t* ret_decoded );


#ifdef __cplusplus
}
#endif

#endif /* _h_utf8_like_int_codec_ */

