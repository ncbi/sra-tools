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

#include "utf8-like-int-codec.h"

#define NOT_ENCODED_SEQUENCE 0xFFu

#define MAX_VALUE_BYTE_1 0x7F
#define MAX_VALUE_BYTE_2 0x7FF
#define MAX_VALUE_BYTE_3 0xFFFF
#define MAX_VALUE_BYTE_4 0x1FFFFF
#define MAX_VALUE_BYTE_5 0x3FFFFFF
#define MAX_VALUE_BYTE_6 0x7FFFFFFF
#define MAX_VALUE_BYTE_7 0xFFFFFFFFF


int encode_uint16 ( uint16_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend )
{
    size_t dst_buf_size = buf_xend - buf_start;
    int ret = CODEC_UNKNOWN_ERROR;

    /* optimization: process 1-byte case in the very beginning */
    if (value_to_encode <= MAX_VALUE_BYTE_1)
    {
        if ( dst_buf_size < 1 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = (uint8_t)value_to_encode;
        ret = 1;
    }
    else if ( value_to_encode <= MAX_VALUE_BYTE_2 )
    {
        /* 1-byte case is processed already, so starting from 2-byte one */
        if ( dst_buf_size < 2 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xC0 | (uint8_t)(value_to_encode >> 6);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 2;
    }
    else /* encoding will take at least 3 bytes which is pointless - writing directly */
    {
        if (dst_buf_size < 3)
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = NOT_ENCODED_SEQUENCE;
        buf_start[1] = (uint8_t)( value_to_encode >> 8 );
        buf_start[2] = (uint8_t)( value_to_encode );

        ret = 3;
    }

    return ret;
}

int decode_uint16 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint16_t* ret_decoded )
{
    size_t src_buf_size = buf_xend - buf_start;
    int ret = CODEC_INVALID_FORMAT;

    if (src_buf_size < 1)
        return CODEC_INSUFFICIENT_BUFFER;

    if ( (int8_t)buf_start[0] >= 0 )
    {
        *ret_decoded = buf_start[0];
        return 1;
    }

    /*neg_ch = ~ buf_start[0];*/
    /*
            neg_ch                |  buf_start[0]
        invalid: 01nnnnnn >= 0x40 | 10xxxxxx < 0xC0
        valid:   
        2-byte:  001nnnnn >= 0x20 | 110xxxxx < 0xE0
        3-byte:  0001nnnn >= 0x10 | 1110xxxx < 0xF0
        4-byte:  00001nnn >= 0x08 | 11110xxx < 0xF8
        5-byte:  000001nn >= 0x04 | 111110xx < 0xFC
        6-byte:  0000001n >= 0x02 | 1111110x < 0xFE
        7-byte:  00000001 >= 0x01 | 11111110 < 0xFF

        So, no need to have neg_ch to achieve the filtering
    */

    /* invalid 1st byte */
    if ( buf_start[0] < 0xC0 )
        return CODEC_INVALID_FORMAT;
    else if ( buf_start[0] < 0xE0 )
    {
        if ( src_buf_size < 2 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x1F;
            uint8_t byte1;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            *ret_decoded  = (uint16_t)byte0 << 6 |
                            (uint16_t)byte1;

            ret = 2;
        }
    }
    else if ( buf_start[0] == NOT_ENCODED_SEQUENCE )
    {
        if ( src_buf_size < 3 )
            return CODEC_INSUFFICIENT_BUFFER;

        *ret_decoded = (uint16_t)buf_start[1] << 8 |
                       (uint16_t)buf_start[2];

        ret = 3;
    }

    return ret;
}


int encode_uint32 ( uint32_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend )
{
    size_t dst_buf_size = buf_xend - buf_start;
    int ret = CODEC_UNKNOWN_ERROR;

    if (value_to_encode <= MAX_VALUE_BYTE_1)
    {
        if ( dst_buf_size < 1 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = (uint8_t)value_to_encode;
        ret = 1;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_2)
    {
        if ( dst_buf_size < 2 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xC0 | (uint8_t)(value_to_encode >> 6);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 2;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_3)
    {
        if ( dst_buf_size < 3 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xE0 | (uint8_t)(value_to_encode >> 12);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode >> 6 & 0x3F);
        buf_start[2] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 3;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_4)
    {
        if ( dst_buf_size < 4 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xF0 | (uint8_t)(value_to_encode >> 18);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode >> 12 & 0x3F);
        buf_start[2] = 0x80 | (uint8_t)(value_to_encode >> 6 & 0x3F);
        buf_start[3] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 4;
    }
    else /* encoding will take at least 5 bytes which is pointless - writing directly */
    {
        if (dst_buf_size < 5)
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = NOT_ENCODED_SEQUENCE;
        buf_start[1] = (uint8_t)( value_to_encode >> 24 );
        buf_start[2] = (uint8_t)( value_to_encode >> 16 );
        buf_start[3] = (uint8_t)( value_to_encode >> 8 );
        buf_start[4] = (uint8_t)( value_to_encode );

        ret = 5;
    }

    return ret;
}

int decode_uint32 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint32_t* ret_decoded )
{
    size_t src_buf_size = buf_xend - buf_start;
    int ret = CODEC_INVALID_FORMAT;

    if (src_buf_size < 1)
        return CODEC_INSUFFICIENT_BUFFER;

    /* 1-byte sequence */
    if ( (int8_t)buf_start[0] >= 0 )
    {
        *ret_decoded = buf_start[0];
        return 1;
    }

    /*neg_ch = ~ buf_start[0];*/
    /*
            neg_ch                |  buf_start[0]
        invalid: 01nnnnnn >= 0x40 | 10xxxxxx < 0xC0
        valid:   
        2-byte:  001nnnnn >= 0x20 | 110xxxxx < 0xE0
        3-byte:  0001nnnn >= 0x10 | 1110xxxx < 0xF0
        4-byte:  00001nnn >= 0x08 | 11110xxx < 0xF8
        5-byte:  000001nn >= 0x04 | 111110xx < 0xFC
        6-byte:  0000001n >= 0x02 | 1111110x < 0xFE
        7-byte:  00000001 >= 0x01 | 11111110 < 0xFF

        So, no need to have neg_ch to achieve the filtering
    */

    /* invalid 1st byte */
    if ( buf_start[0] < 0xC0 )
        return CODEC_INVALID_FORMAT;
    /* 2-byte sequence */
    else if ( buf_start[0] < 0xE0 )
    {
        if ( src_buf_size < 2 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x1F;
            uint8_t byte1;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            *ret_decoded  = (uint32_t)byte0 << 6 |
                            (uint32_t)byte1;

            ret = 2;
        }
    }
    /* 3-byte sequence */
    else if ( buf_start[0] < 0xF0 )
    {
        if ( src_buf_size < 3 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x0F;
            uint8_t byte1, byte2;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            if ( ( buf_start[2] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte2 = buf_start[2] & 0x3F;

            *ret_decoded  = (uint32_t)byte0 << 12 |
                            (uint32_t)byte1 << 6 |
                            (uint32_t)byte2;

            ret = 3;
        }
    }
    /* 4-byte sequence */
    else if ( buf_start[0] < 0xF8 )
    {
        if  ( src_buf_size < 4 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x07;
            uint8_t byte1, byte2, byte3;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            if ( ( buf_start[2] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte2 = buf_start[2] & 0x3F;

            if ( ( buf_start[3] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte3 = buf_start[3] & 0x3F;

            *ret_decoded  = (uint32_t)byte0 << 18 |
                            (uint32_t)byte1 << 12 |
                            (uint32_t)byte2 << 6 |
                            (uint32_t)byte3;

            ret = 4;
        }
    }
    else if ( buf_start[0] == NOT_ENCODED_SEQUENCE )
    {
        if ( src_buf_size < 5 )
            return CODEC_INSUFFICIENT_BUFFER;

        *ret_decoded = (uint32_t)buf_start[1] << 24 |
                       (uint32_t)buf_start[2] << 16 |
                       (uint32_t)buf_start[3] << 8 |
                       (uint32_t)buf_start[4];

        ret = 5;
    }

    return ret;
}


int encode_uint64 ( uint64_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend )
{
    size_t dst_buf_size = buf_xend - buf_start;
    int ret = CODEC_UNKNOWN_ERROR;

    if (value_to_encode <= MAX_VALUE_BYTE_1)
    {
        if ( dst_buf_size < 1 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = (uint8_t)value_to_encode;
        ret = 1;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_2)
    {
        if ( dst_buf_size < 2 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xC0 | (uint8_t)(value_to_encode >> 6);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 2;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_3)
    {
        if ( dst_buf_size < 3 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xE0 | (uint8_t)(value_to_encode >> 12);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode >> 6 & 0x3F);
        buf_start[2] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 3;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_4)
    {
        if ( dst_buf_size < 4 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xF0 | (uint8_t)(value_to_encode >> 18);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode >> 12 & 0x3F);
        buf_start[2] = 0x80 | (uint8_t)(value_to_encode >> 6 & 0x3F);
        buf_start[3] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 4;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_5)
    {
        if ( dst_buf_size < 5 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xF8 | (uint8_t)(value_to_encode >> 24);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode >> 18 & 0x3F);
        buf_start[2] = 0x80 | (uint8_t)(value_to_encode >> 12 & 0x3F);
        buf_start[3] = 0x80 | (uint8_t)(value_to_encode >> 6 & 0x3F);
        buf_start[4] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 5;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_6)
    {
        if ( dst_buf_size < 6 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xFC | (uint8_t)(value_to_encode >> 30);
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode >> 24 & 0x3F);
        buf_start[2] = 0x80 | (uint8_t)(value_to_encode >> 18 & 0x3F);
        buf_start[3] = 0x80 | (uint8_t)(value_to_encode >> 12 & 0x3F);
        buf_start[4] = 0x80 | (uint8_t)(value_to_encode >> 6 & 0x3F);
        buf_start[5] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 6;
    }
    else if (value_to_encode <= MAX_VALUE_BYTE_7)
    {
        if ( dst_buf_size < 7 )
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = 0xFE;/* | (uint8_t)(value_to_encode >> 36);*/ /* this shift will always give 0 */
        buf_start[1] = 0x80 | (uint8_t)(value_to_encode >> 30 & 0x3F);
        buf_start[2] = 0x80 | (uint8_t)(value_to_encode >> 24 & 0x3F);
        buf_start[3] = 0x80 | (uint8_t)(value_to_encode >> 18 & 0x3F);
        buf_start[4] = 0x80 | (uint8_t)(value_to_encode >> 12 & 0x3F);
        buf_start[5] = 0x80 | (uint8_t)(value_to_encode >> 6 & 0x3F);
        buf_start[6] = 0x80 | (uint8_t)(value_to_encode & 0x3F);

        ret = 7;
    }
    else /* there is a limitation - cannot encode in more than 7 bytes, writing directly now */
    {
        if (dst_buf_size < 9)
            return CODEC_INSUFFICIENT_BUFFER;

        buf_start[0] = NOT_ENCODED_SEQUENCE;
        buf_start[1] = (uint8_t)( value_to_encode >> 56 );
        buf_start[2] = (uint8_t)( value_to_encode >> 48 );
        buf_start[3] = (uint8_t)( value_to_encode >> 40 );
        buf_start[4] = (uint8_t)( value_to_encode >> 32 );
        buf_start[5] = (uint8_t)( value_to_encode >> 24 );
        buf_start[6] = (uint8_t)( value_to_encode >> 16 );
        buf_start[7] = (uint8_t)( value_to_encode >> 8 );
        buf_start[8] = (uint8_t)( value_to_encode );

        ret = 9;
    }

    return ret;
}

int decode_uint64 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint64_t* ret_decoded )
{
    size_t src_buf_size = buf_xend - buf_start;
    int ret = CODEC_INVALID_FORMAT;

    if (src_buf_size < 1)
        return CODEC_INSUFFICIENT_BUFFER;

    /* 1-byte sequence */
    if ( (int8_t)buf_start[0] >= 0 )
    {
        *ret_decoded = buf_start[0];
        return 1;
    }

    /*neg_ch = ~ buf_start[0];*/
    /*
            neg_ch                |  buf_start[0]
        invalid: 01nnnnnn >= 0x40 | 10xxxxxx < 0xC0
        valid:   
        2-byte:  001nnnnn >= 0x20 | 110xxxxx < 0xE0
        3-byte:  0001nnnn >= 0x10 | 1110xxxx < 0xF0
        4-byte:  00001nnn >= 0x08 | 11110xxx < 0xF8
        5-byte:  000001nn >= 0x04 | 111110xx < 0xFC
        6-byte:  0000001n >= 0x02 | 1111110x < 0xFE
        7-byte:  00000001 >= 0x01 | 11111110 < 0xFF

        So, no need to have neg_ch to achieve the filtering
    */

    /* invalid 1st byte */
    if ( buf_start[0] < 0xC0 )
        return CODEC_INVALID_FORMAT;
    /* 2-byte sequence */
    else if ( buf_start[0] < 0xE0 )
    {
        if ( src_buf_size < 2 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x1F;
            uint8_t byte1;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            *ret_decoded  = (uint64_t)byte0 << 6 |
                            (uint64_t)byte1;

            ret = 2;
        }
    }
    /* 3-byte sequence */
    else if ( buf_start[0] < 0xF0 )
    {
        if ( src_buf_size < 3 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x0F;
            uint8_t byte1, byte2;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            if ( ( buf_start[2] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte2 = buf_start[2] & 0x3F;

            *ret_decoded  = (uint64_t)byte0 << 12 |
                            (uint64_t)byte1 << 6 |
                            (uint64_t)byte2;

            ret = 3;
        }
    }
    /* 4-byte sequence */
    else if ( buf_start[0] < 0xF8 )
    {
        if  ( src_buf_size < 4 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x07;
            uint8_t byte1, byte2, byte3;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            if ( ( buf_start[2] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte2 = buf_start[2] & 0x3F;

            if ( ( buf_start[3] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte3 = buf_start[3] & 0x3F;

            *ret_decoded  = (uint64_t)byte0 << 18 |
                            (uint64_t)byte1 << 12 |
                            (uint64_t)byte2 << 6 |
                            (uint64_t)byte3;

            ret = 4;
        }
    }
    /* 5-byte sequence */
    else if ( buf_start[0] < 0xFC )
    {
        if  ( src_buf_size < 5 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x03;
            uint8_t byte1, byte2, byte3, byte4;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            if ( ( buf_start[2] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte2 = buf_start[2] & 0x3F;

            if ( ( buf_start[3] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte3 = buf_start[3] & 0x3F;

            if ( ( buf_start[4] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte4 = buf_start[4] & 0x3F;

            *ret_decoded  = (uint64_t)byte0 << 24 |
                            (uint64_t)byte1 << 18 |
                            (uint64_t)byte2 << 12 |
                            (uint64_t)byte3 << 6 |
                            (uint64_t)byte4;

            ret = 5;
        }
    }
    /* 6-byte sequence */
    else if ( buf_start[0] < 0xFE )
    {
        if  ( src_buf_size < 6 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte0 = buf_start[0] & 0x01;
            uint8_t byte1, byte2, byte3, byte4, byte5;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            if ( ( buf_start[2] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte2 = buf_start[2] & 0x3F;

            if ( ( buf_start[3] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte3 = buf_start[3] & 0x3F;

            if ( ( buf_start[4] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte4 = buf_start[4] & 0x3F;

            if ( ( buf_start[5] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte5 = buf_start[5] & 0x3F;

            *ret_decoded  = (uint64_t)byte0 << 30 |
                            (uint64_t)byte1 << 24 |
                            (uint64_t)byte2 << 18 |
                            (uint64_t)byte3 << 12 |
                            (uint64_t)byte4 << 6 |
                            (uint64_t)byte5;

            ret = 6;
        }
    }
    /* 6-byte sequence */
    else if ( buf_start[0] == 0xFE )
    {
        if  ( src_buf_size < 7 )
            return CODEC_INSUFFICIENT_BUFFER;
        else
        {
            uint8_t byte1, byte2, byte3, byte4, byte5, byte6;

            if ( ( buf_start[1] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte1 = buf_start[1] & 0x3F;

            if ( ( buf_start[2] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte2 = buf_start[2] & 0x3F;

            if ( ( buf_start[3] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte3 = buf_start[3] & 0x3F;

            if ( ( buf_start[4] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte4 = buf_start[4] & 0x3F;

            if ( ( buf_start[5] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte5 = buf_start[5] & 0x3F;

            if ( ( buf_start[6] & 0xC0 ) != 0x80 )
                return CODEC_INVALID_FORMAT;
            byte6 = buf_start[6] & 0x3F;

            *ret_decoded  = (uint64_t)byte1 << 30 |
                            (uint64_t)byte2 << 24 |
                            (uint64_t)byte3 << 18 |
                            (uint64_t)byte4 << 12 |
                            (uint64_t)byte5 << 6 |
                            (uint64_t)byte6;

            ret = 7;
        }
    }
    else /* if ( buf_start[0] == NOT_ENCODED_SEQUENCE ) */
    {
        if ( src_buf_size < 9 )
            return CODEC_INSUFFICIENT_BUFFER;

        *ret_decoded = (uint64_t)buf_start[1] << 56 |
                       (uint64_t)buf_start[2] << 48 |
                       (uint64_t)buf_start[3] << 40 |
                       (uint64_t)buf_start[4] << 32 |
                       (uint64_t)buf_start[5] << 24 |
                       (uint64_t)buf_start[6] << 16 |
                       (uint64_t)buf_start[7] << 8 |
                       (uint64_t)buf_start[8];

        ret = 9;
    }

    return ret;
}
