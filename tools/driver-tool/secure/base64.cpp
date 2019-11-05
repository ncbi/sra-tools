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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#include <ncbi/secure/base64.hpp>

// auto-generated tables
#include "base64-tables.hpp"

#include <cstdint>
#include <cassert>

namespace ncbi
{
#if 0
    // from binary 0..63 to standard BASE64 encoding
    static
    const char encode_std_table [] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "+/";

    // from octet stream ( presumed standard BASE64 encoding )
    // to binary, where
    //   0..63 is valid output
    //  -1 is invalid output
    //  -2 is padding
    static
    const char decode_std_table [] =
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x00 .. \x0F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x10 .. \x1F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x3e\xff\xff\xff\x3f" // \x20 .. \x2F
        "\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\xff\xff\xff\xfe\xff\xff" // \x30 .. \x3F
        "\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e" // \x40 .. \x4F
        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xff\xff\xff\xff\xff" // \x50 .. \x5F
        "\xff\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28" // \x60 .. \x6F
        "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\xff\xff\xff\xff\xff" // \x70 .. \x7F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x80 .. \x8F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x90 .. \x9F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xA0 .. \xAF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xB0 .. \xBF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xC0 .. \xCF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xD0 .. \xDF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xE0 .. \xEF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xF0 .. \xFF
        ;

    // from octet stream ( presumed standard BASE64 encoding )
    // to binary, where
    //   0..63 is valid output
    //  -1 is invalid output
    //  -2 is padding
    //  -3 means ignore whitespace ( invalid within JWT )
    static
    const char decode_std_table_ws [] =
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\xfd\xff\xff\xfd\xff\xff" // \x00 .. \x0F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x10 .. \x1F
        "\xfd\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x3e\xff\xff\xff\x3f" // \x20 .. \x2F
        "\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\xff\xff\xff\xfe\xff\xff" // \x30 .. \x3F
        "\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e" // \x40 .. \x4F
        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xff\xff\xff\xff\xff" // \x50 .. \x5F
        "\xff\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28" // \x60 .. \x6F
        "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\xff\xff\xff\xff\xff" // \x70 .. \x7F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x80 .. \x8F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x90 .. \x9F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xA0 .. \xAF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xB0 .. \xBF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xC0 .. \xCF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xD0 .. \xDF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xE0 .. \xEF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xF0 .. \xFF
        ;
    
    // from binary 0..63 to BASE64-URL encoding
    static
    const char encode_url_table [] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "-_";
    
    // from octet stream ( presumed BASE64-URL encoding )
    // to binary, where
    //   0..63 is valid output
    //  -1 is invalid output
    //  -2 is padding
    //  -3 would normally mean ignore ( invalid within JWT )
    static
    const char decode_url_table [] =
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x00 .. \x0F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x10 .. \x1F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x3e\xff\xff" // \x20 .. \x2F
        "\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\xff\xff\xff\xfe\xff\xff" // \x30 .. \x3F
        "\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e" // \x40 .. \x4F
        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xff\xff\xff\xff\x3f" // \x50 .. \x5F
        "\xff\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28" // \x60 .. \x6F
        "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\xff\xff\xff\xff\xff" // \x70 .. \x7F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x80 .. \x8F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x90 .. \x9F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xA0 .. \xAF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xB0 .. \xBF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xC0 .. \xCF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xD0 .. \xDF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xE0 .. \xEF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xF0 .. \xFF
        ;
    
    // from octet stream ( presumed BASE64-URL encoding )
    // to binary, where
    //   0..63 is valid output
    //  -1 is invalid output
    //  -2 is padding
    //  -3 would normally mean ignore ( invalid within JWT )
    static
    const char decode_url_table_ws [] =
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\xfd\xff\xff\xfd\xff\xff" // \x00 .. \x0F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x10 .. \x1F
        "\xfd\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x3e\xff\xff" // \x20 .. \x2F
        "\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\xff\xff\xff\xfe\xff\xff" // \x30 .. \x3F
        "\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e" // \x40 .. \x4F
        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xff\xff\xff\xff\x3f" // \x50 .. \x5F
        "\xff\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28" // \x60 .. \x6F
        "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\xff\xff\xff\xff\xff" // \x70 .. \x7F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x80 .. \x8F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \x90 .. \x9F
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xA0 .. \xAF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xB0 .. \xBF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xC0 .. \xCF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xD0 .. \xDF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xE0 .. \xEF
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" // \xF0 .. \xFF
        ;
#endif
    
    static
    const String encodeBase64Impl ( const void * data, size_t bytes, const char encode_table [] )
    {
        // allow an empty source
        if ( bytes == 0 )
            return "";
        
        // this exception represents an internal error in any case
        if ( data == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "invalid payload" );

        // TBD - place an upper limit on data size
        // a very large payload will create a very large string allocation
        // and may indicate garbage that could result in a segfault
        if ( bytes >= 0x40000000 )
        {
            throw SizeViolation (
                XP ( XLOC )
                << "payload too large: "
                << bytes
                << " bytes"
                );
        }

        // gather encoded output in a string
        // this is why we wanted to limit the data size
        StringBuffer encoding;

        // perform work in a stack-local buffer to reduce
        // memory manager thrashing when adding to "encoding" string
        size_t i, j;
        char buff [ 4096 ];

        // accumulate 24 bits of input at a time into a 32-bit accumulator
        uint32_t acc;

        // walk across data as a block of octets
        const unsigned char * js = ( const unsigned char * ) data;

        // consume 3 octets of input while producing 4 output characters
        // JWT does not allow formatting, so no line breaks are involved
        for ( i = j = 0; i + 3 <= bytes; i += 3, j += 4 )
        {
            // bring in 24 bits in specific order
            acc
            = ( ( uint32_t ) js [ i + 0 ] << 16 )
            | ( ( uint32_t ) js [ i + 1 ] <<  8 )
            | ( ( uint32_t ) js [ i + 2 ] <<  0 )
            ;

            // we need to emit 4 bytes of output
            // flush the local buffer if it cannot hold them all
            if ( j > ( sizeof buff - 4 ) )
            {
                encoding += String ( buff, j );
                j = 0;
            }

            // produce the 4 bytes of output through the provided encoding table
            // each index MUST be 0..63
            buff [ j + 0 ] = encode_table [ ( acc >> 18 ) & 0x3F ];
            buff [ j + 1 ] = encode_table [ ( acc >> 12 ) & 0x3F ];
            buff [ j + 2 ] = encode_table [ ( acc >>  6 ) & 0x3F ];
            buff [ j + 3 ] = encode_table [ ( acc >>  0 ) & 0x3F ];
        }

        // at this point, the data block is either completely converted
        // or has 1 or 2 bytes left over, since ( bytes % 3 ) is in { 0, 1, 2 }
        // we know "i" is a multiple of 3, and ( bytes - i ) == ( bytes % 3 )
        switch ( bytes - i )
        {
            case 0:
                // everything is complete
                break;
                
            case 1:

                // 1 octet left over
                // place it in the highest part of 24-bit accumulator
                acc
                = ( ( uint32_t ) js [ i + 0 ] << 16 )
                ;

                // we need to emit 4 bytes of output
                // flush buffer if insufficient space is available
                if ( j > ( sizeof buff - 4 ) )
                {
                    encoding += String ( buff, j );
                    j = 0;
                }

                // emit single octet split between two encoding characters
                buff [ j + 0 ] = encode_table [ ( acc >> 18 ) & 0x3F ];
                buff [ j + 1 ] = encode_table [ ( acc >> 12 ) & 0x3F ];

                // pad the remaining two with padding character
                buff [ j + 2 ] = '=';
                buff [ j + 3 ] = '=';

#if BASE64_PAD_ENCODING
                // total number of characters in buffer includes padding
                j += 4;
#else
                // total number of characters in buffer does not include padding
                j += 2;
#endif
                
                break;
                
            case 2:
                
                // 2 octets left over
                // place them in the upper part of 24-bit accumulator
                acc
                = ( ( uint32_t ) js [ i + 0 ] << 16 )
                | ( ( uint32_t ) js [ i + 1 ] <<  8 )
                ;

                // test for buffer space as above
                if ( j > ( sizeof buff - 4 ) )
                {
                    encoding += String ( buff, j );
                    j = 0;
                }

                // emit the two octets split across three encoding characters
                buff [ j + 0 ] = encode_table [ ( acc >> 18 ) & 0x3F ];
                buff [ j + 1 ] = encode_table [ ( acc >> 12 ) & 0x3F ];
                buff [ j + 2 ] = encode_table [ ( acc >>  6 ) & 0x3F ];

                // pad the remainder with padding character
                buff [ j + 3 ] = '=';
                
#if BASE64_PAD_ENCODING
                // total number of characters in buffer includes padding
                j += 4;
#else
                // total number of characters in buffer does not include padding
                j += 3;
#endif
                
                break;
                
            default:

                // this is theoretically impossible
                throw InternalError ( XP ( XLOC ) << "1 - aaaah!" );
        }

        // if "j" is not 0 at this point, it means
        // there are encoding characters in the stack-local buffer
        // append them to the encoding string
        if ( j != 0 )
            encoding += String ( buff, j );

        // done.
        return encoding . stealString ();
    }
    
    static
    const Payload decodeBase64Impl ( const String & encoding, const char decode_table [] )
    {
        // base the estimate of decoded size on input size
        // this can be over-estimated due to embedded padding or formatting characters
        // however, these are prohibited in JWT so the size should be nearly exact
        size_t i, j, len = encoding . size ();
        
        uint32_t acc, ac;
        const unsigned char * en = ( const unsigned char * ) encoding . data ();
        
        // the returned buffer should be 3/4 the size of the input string,
        // provided that there are no padding bytes in the input
        size_t bytes = ( ( len + 3 ) / 4 ) * 3;
        
        // create an output buffer
        Payload payload ( bytes );
        unsigned char * buff = payload . data ();

        // generate a capacity limit, beyond which
        // the buffer must be extended
        size_t cap_limit = payload . capacity () - 3;

        // walk across the input string a byte at a time
        // avoid temptation to consume 4 bytes at a time,
        // in order to be robust to any allowed stray characters
        // NB - if this proves to be a performance issue, it can
        // be optimized in the future.
        for ( i = j = 0, acc = ac = 0; i < len; ++ i )
        {
            // return from table is a SIGNED entity
            int byte = decode_table [ en [ i ] ];

            // non-negative lookups are valid translations
            if ( byte >= 0 )
            {
                // must be 0..63
                assert ( byte < 64 );

                // shift 6 bits into accumulator
                acc <<= 6;
                acc |= byte;

                // if the number of codes in accumulator is 4 ( i.e. 24 bits )
                if ( ++ ac == 4 )
                {
                    // test capacity of output
                    if ( j > cap_limit )
                    {
                        payload . increaseCapacity ();
                        buff = payload . data ();
                        cap_limit = payload . capacity () - 3;
                    }

                    // put 3 octets into payload
                    buff [ j + 0 ] = ( unsigned char ) ( acc >> 16 );
                    buff [ j + 1 ] = ( unsigned char ) ( acc >>  8 );
                    buff [ j + 2 ] = ( unsigned char ) ( acc >>  0 );

                    // clear the accumulator
                    ac = 0;

                    // keep track of size
                    // NB - this is not YET reflected in payload
                    j += 3;
                }
            }

            // NEGATIVE lookups have to be interpreted
            else
            {
                // the special value -2 means padding
                // which indicates the end of the input
                if ( byte == -2 )
                    break;

                // the special value -3 would indicate ignore
                // but it's not allowed in JWT and so is not expected to be in table
                // any other value ( notably -1 ) is illegal
                if ( byte != -3 )
                    throw InvalidInputException ( XP ( XLOC ) << "illegal input characters" );
            }
        }

        // test the number of 6-bit codes remaining in the accumulator
        switch ( ac )
        {
            case 0:
                // none - everything has been converted
                break;
                
            case 1:
                // encoding granularity is an octet - 8 bits
                // it MUST be split across two codes - 6 bits each, i.e. 12 bits
                // having only 6 bits in accumulator is illegal
                throw InvalidInputException ( XP ( XLOC ) << "malformed input - group with 1 base64 encode character" );
                
            case 2:
                
                // fill accumulator with two padding codes
                // NB - not strictly necessary, but keeps code regular and readable
                acc <<= 12;
                
                // check buffer for space
                if ( j >= payload . capacity () )
                {
                    payload . increaseCapacity ( 1 );
                    buff = payload . data ();
                }
                
                // set additional octet
                buff [ j + 0 ] = ( char ) ( acc >> 16 );

                // account for size
                // NB - this is not YET reflected in payload
                j += 1;
                break;
                
            case 3:
                
                // fill accumulator with padding
                acc <<= 6;
                
                // check buffer for space
                if ( j + 1 >= payload . capacity () )
                {
                    payload . increaseCapacity ( 2 );
                    buff = payload . data ();
                }

                // set additional bytes
                buff [ j + 0 ] = ( char ) ( acc >> 16 );
                buff [ j + 1 ] = ( char ) ( acc >>  8 );
                
                j += 2;
                break;
                
            default:

                // theoretically impossible
                throw InternalError ( XP ( XLOC ) << "2 - aaah!" );
        }

        // NOW store size on object
        payload . setSize ( j );

        // return the object
        return payload;
    }

    static
    const String payloadToUTF8 ( const Payload & payload )
    {
        const unsigned char * buff = payload . data ();
        size_t size = payload . size ();
        return String ( ( const char * ) buff, size );
    }
    
    const String Base64 :: encode ( const void * data, size_t bytes )
    {
        return encodeBase64Impl ( data, bytes, encode_std_table );
    }
    
    const Payload Base64 :: decode ( const String & encoding, bool allow_whitespace )
    {
        Payload payload = decodeBase64Impl ( encoding,
            allow_whitespace ? decode_std_table_ws : decode_std_table );
        return payload;
    }
    
    const String Base64 :: decodeText ( const String & encoding, bool allow_whitespace )
    {
        Payload payload = decodeBase64Impl ( encoding,
            allow_whitespace ? decode_std_table_ws : decode_std_table );
        return payloadToUTF8 ( payload );
    }
    
    const String Base64 :: urlEncode ( const void * data, size_t bytes )
    {
        return encodeBase64Impl ( data, bytes, encode_url_table );
    }
    
    const Payload Base64 :: urlDecode ( const String & encoding, bool allow_whitespace )
    {
        return decodeBase64Impl ( encoding,
            allow_whitespace ? decode_url_table_ws : decode_url_table );
    }
    
    const String Base64 :: urlDecodeText ( const String & encoding, bool allow_whitespace )
    {
        Payload payload = decodeBase64Impl ( encoding,
            allow_whitespace ? decode_url_table_ws : decode_url_table );
        return payloadToUTF8 ( payload );
    }
    
}
