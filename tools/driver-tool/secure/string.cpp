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

#include <ncbi/secure/string.hpp>
#include <ncbi/secure/payload.hpp>
#include "memset-priv.hpp"
#include "utf8proc.h"

#include <cstdint>
#include <cstring>
#include <cassert>
#include <locale>
#include <string>
#include <ctype.h>

#if MAC
#include <architecture/byte_order.h>
#define bswap_16(x) OSSwapInt16 (x)
#define bswap_32(x) OSSwapInt32 (x)
#define bswap_64(x) OSSwapInt64 (x)
#else
#include <byteswap.h>
#endif

namespace ncbi
{

    const count_t simple_search_edge = 256;
    const count_t nsquare_search_edge = 64;


    /*=================================================*
     *                     UTILITY                     *
     *=================================================*/

    /* is_ascii
     *  easier to check with constant code than using lookup table
     */
    static inline
    bool is_ascii ( UTF32 ch )
    {
        return ch < 128;
    }

    /* is_ascii
     *  scan a NUL-terminated string for presence of any non-ASCII characters.
     *  returns false if non-ASCII is found
     *  returns false if pointer is NULL
     *  returns true otherwise, i.e. an empty string is called ASCII.
     */
    static
    bool is_ascii ( const UTF8 * zstr )
    {
        // a null pointer isn't anything
        if ( zstr == nullptr )
            return false;

        // assume ASCII, return false on anything non-ASCII
        for ( size_t i = 0; zstr [ i ] != 0; ++ i )
        {
            if ( zstr [ i ] < 0 )
                return false;
        }

        return true;
    }

    static
    bool is_ascii ( const UTF8 * str, size_t bytes )
    {
        // a null pointer isn't anything
        if ( str == nullptr )
            return false;

        // assume ASCII, return false on anything non-ASCII
        for ( size_t i = 0; i < bytes; ++ i )
        {
            if ( str [ i ] < 0 )
                return false;
        }

        return true;
    }

    /* scan_fwd
     *  used mainly by iterators to step across a single character
     *  returns the number of bytes advanced
     *  for cases where the offset was already out of bounds, advance by 1
     */
    static
    long int scan_fwd ( const UTF8 * data, size_t bytes, size_t offset )
    {
        if ( offset < 0 || offset >= bytes )
            return 1;

        if ( data [ offset ] >= 0 )
            return 1;

        unsigned int ch = data [ offset ];
        return __builtin_clz ( ~ ( ch << 24 ) );
    }

    /* scan_rev
     *  used by iterators to back up by a single character
     *  returns the number of bytes backed over
     *  for cases where the offset was already out of bounds, back up by 1
     */
    static
    long int scan_rev ( const UTF8 * data, size_t bytes, size_t offset )
    {
        if ( offset <= 0 || offset > bytes )
            return 1;
    
        if ( data [ offset - 1 ] >= 0 )
            return 1;

        long int clen = 1;
        for ( ; offset > 0; ++ clen )
        {
            if ( ( data [ -- offset ] & 0xC0 ) != 0x80 )
                break;
        }

        unsigned int ch = data [ offset ];
        if ( clen != __builtin_clz ( ~ ( ch << 24 ) ) )
            throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

        return clen;
    }


    /* string_elems
     *  measure a NUL-terminated string in elements
     */
    template < class T >
    static inline count_t string_elems ( const T * zstr )
    {
        size_t i;
        for ( i = 0; zstr [ i ] != 0; ++ i )
            ( void ) 0;
        return i;
    }

    /* string_size
     *  measure a NUL-terminated string in bytes
     */
    static inline
    size_t string_size ( const char * zstr )
    {
        return string_elems < char > ( zstr );
    }

    /* string_length
     *  measure the string in characters
     */
    static
    count_t string_length ( const UTF8 * data, size_t bytes )
    {
        // allow empty string
        count_t cnt = 0;

        // walk across all bytes
        for ( size_t i = 0; i < bytes; ++ cnt, ++ i )
        {
            // skip over ASCII; drop into UTF-8
            if ( data [ i ] < 0 )
            {
                // create word from leading byte
                unsigned int ch = data [ i ];

                // remember starting position
                size_t start = i;

                // by inverting leading byte and counting leading zeros,
                // we should find exactly the number of bytes in char
                size_t clen = __builtin_clz ( ~ ( ch << 24 ) );
                if ( clen < 2 || clen > 4 )
                    throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

                // walk across non-leading bytes
                for ( ++ i; i < bytes; ++ i )
                {
                    // any that is not 0x80 | 6-bits ends char
                    if ( ( data [ i ] & 0xC0 ) != 0x80 )
                        break;
                }

                // the total length of multi-byte char should match
                if ( i - start != clen )
                    throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

                // back up in anticipation of loop increment
                --i;
            }
        }

        return cnt;
    }

    static
    count_t string_length ( const UTF16 * data, count_t elems )
    {
        count_t cnt = 0;

        if ( elems != 0 )
        {
            for ( count_t i = 0; i < elems; ++ cnt, ++ i )
            {
                UTF16 ch = data [ i ];
                if ( ch >= 0xD800 && ch <= 0xDFFF )
                {
                    if ( ( ch & 0xDC00 ) != 0xD800 || i + 1 == elems )
                        throw InvalidUTF16String ( XP ( XLOC ) << "badly formed UTF-16 surrogate pair" );
                    ch = data [ i + 1 ];
                    if ( ( ch & 0xDC00 ) != 0xDC00 )
                        throw InvalidUTF16String ( XP ( XLOC ) << "badly formed UTF-16 surrogate pair" );
                    ++ i;
                }
            }
        }

        return cnt;
    }

    static
    count_t string_length_bswap ( const UTF16 * data, count_t elems )
    {
        count_t cnt = 0;

        if ( elems != 0 )
        {
            for ( count_t i = 0; i < elems; ++ cnt, ++ i )
            {
                UTF16 ch = bswap_16 ( data [ i ] );
                if ( ch >= 0xD800 && ch <= 0xDFFF )
                {
                    if ( ( ch & 0xDC00 ) != 0xD800 || i + 1 == elems )
                        throw InvalidUTF16String ( XP ( XLOC ) << "badly formed UTF-16 surrogate pair" );
                    ch = bswap_16 ( data [ i + 1 ] );
                    if ( ( ch & 0xDC00 ) != 0xDC00 )
                        throw InvalidUTF16String ( XP ( XLOC ) << "badly formed UTF-16 surrogate pair" );
                    ++ i;
                }
            }
        }

        return cnt;
    }

    /* char_idx_to_byte_off
     *  linearly scan a UTF-8 string to find a character
     *  return the byte offset from the beginning of the string
     */
    static
    size_t char_idx_to_byte_off ( const UTF8 * data, size_t bytes, count_t idx )
    {
        assert ( idx <= bytes );

        count_t cnt = 0;
        for ( size_t i = 0; i < bytes; ++ cnt, ++ i )
        {
            // see if this is the spot
            if ( cnt == idx )
                return i;

            // detect non-ASCII character
            // will have bit 7 set
            if ( data [ i ] < 0 )
            {
                unsigned int ch = data [ i ];
                unsigned int clen = __builtin_clz ( ~ ( ch << 24 ) );

                // this may not be necessary for pre-validated strings
                // without it, however, there is no guarantee of advancement
                // across the string or exit from the loop.
                if ( clen < 2 || clen > 4 )
                    throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

                i += clen - 1;
            }
        }

        throw InvalidUTF8String ( XP ( XLOC ) << "character count mismatch" );
    }

    /* utf8_to_utf32
     *  gather 1..4 bytes of UTF-8 into a single UTF-32 character
     *  input data are trusted to have been pre-validated.
     *
     *  at present, there are guards in there to assert invariants,
     *  but they may be conditionally relaxed for performance if deemed
     *  a significant improvement.
     */
    static
    UTF32 utf8_to_utf32 ( const UTF8 * data, size_t bytes, size_t offset )
    {
        // should have been checked by caller
        if ( offset >= bytes )
            throw BoundsException ( XP ( XLOC ) << "offset leaves no data" );

        // detect ASCII
        if ( data [ offset ] >= 0 )
            return data [ offset ];

        // ease of typecast
        const unsigned char * p = reinterpret_cast < const unsigned char * > ( data + offset );

        // determine UTF-8 character length
        UTF32 ch = p [ 0 ];
        unsigned int b2, b3, b4;
        unsigned int clen = __builtin_clz ( ~ ( ch << 24 ) );

        // should have already been pre-validated
        // this test could be removed if deemed significant
        // but then, there are much faster algorithms if performance is at issue
        if ( offset + clen > bytes )
            throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

        // all characters are at least 2 bytes
        b2 = p [ 1 ];

        // mechanically assemble the code
        // assuming that correctness has already been verified
        switch ( clen )
        {
        case 2:
            ch = ( ( ch & 0x1F ) << 6 ) | ( b2 & 0x3F );
            break;
        case 3:
            b3 = p [ 2 ];
            ch = ( ( ch & 0x0F ) << 12 ) | ( ( b2 & 0x3F ) << 6 ) | ( b3 & 0x3F );
            break;
        case 4:
            b3 = p [ 2 ];
            b4 = p [ 3 ];
            ch = ( ( ch & 0x07 ) << 18 ) | ( ( b2 & 0x3F ) << 12 ) | ( ( b3 & 0x3F ) << 6 ) | ( b4 & 0x3F );
            break;
        default:
            // should never happen
            throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
        }

        return ch;
    }


    /* utf8_to_utf32_strict
     *  gather 1..4 bytes of UTF-8 into a single UTF-32 character, and
     *  reject any input that does not pass validation.
     */
    struct UniChar
    {
        UniChar ( UTF32 _ch, count_t _elems )
            : ch ( _ch )
            , elems ( _elems )
        {
        }

        UTF32 ch;
        count_t elems;
    };

    static
    UniChar utf8_to_utf32_strict ( const UTF8 * data, size_t bytes, size_t offset )
    {
        // UTF-8 characters are formed with 1..4 bytes
        // the format supports up to 6 bytes

        // TBD - there are faster ways of doing this

        if ( offset >= bytes )
            throw BoundsException ( XP ( XLOC ) << "offset leaves no data" );

        // bytes guaranteed > 0
        bytes -= offset;

        // ease of typecast
        const unsigned char * p = reinterpret_cast < const unsigned char * > ( data + offset );

        // get the leading byte
        UTF32 ch = p [ 0 ];

        // assume size of 1
        size_t len = 1;

        // detect multi-byte
        if ( ch >= 0x80 )
        {
            // auxiliary bytes
            unsigned int b2, b3, b4;

            // detect 2 byte
            if ( ch < 0xE0 )
            {
                len = 2;
                if ( bytes < 2 )
                    throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

                b2 = p [ 1 ];

                // leading byte must be in 0xC2..0xDF
                if ( ch < 0xC2 || ( b2 & 0xC0 ) != 0x80 )
                    throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

                // compose character
                // valid UTF-16 or UTF-32
                ch = ( ( ch & 0x1F ) << 6 ) | ( b2 & 0x3F );
            }

            // detect 3 byte
            else if ( ch < 0xF0 )
            {
                len = 3;
                if ( bytes < 3 )
                    throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

                b2 = p [ 1 ];
                b3 = p [ 2 ];

                // case for leading byte of exactly 0xE0
                if ( ch == 0xE0 )
                {
                    if ( b2 < 0xA0 || b2 > 0xBF || ( b3 & 0xC0 ) != 0x80 )
                        throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
                }

                // case for leading byte in 0xE1..0xEC
                else if ( ch < 0xED )
                {
                    if ( ( b2 & 0xC0 ) != 0x80 || ( b3 & 0xC0 ) != 0x80 )
                        throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
                }

                // case for leading byte of exactly 0xED
                else if ( ch == 0xED )
                {
                    if ( b2 < 0x80 || b2 > 0x9F || ( b3 & 0xC0 ) != 0x80 )
                        throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
                }

                // case for leading byte in 0xEE..0xEF
                else
                {
                    if ( ( b2 & 0xC0 ) != 0x80 || ( b3 & 0xC0 ) != 0x80 )
                        throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
                }

                // compose character
                // valid UTF-16 or UTF-32
                ch = ( ( ch & 0x0F ) << 12 ) | ( ( b2 & 0x3F ) << 6 ) | ( b3 & 0x3F );
            }

            // detect 4 byte
            else if ( ch <= 0xF4 )
            {
                len = 4;
                if ( bytes < 4 )
                    throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );

                b2 = p [ 1 ];
                b3 = p [ 2 ];
                b4 = p [ 3 ];

                // case for leading byte of exactly 0xF0
                if ( ch == 0xF0 )
                {
                    if ( b2 < 0x90 || b2 > 0xBF || ( b3 & 0xC0 ) != 0x80 || ( b4 & 0xC0 ) != 0x80 )
                        throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
                }

                // case for leading byte in 0xE1..0xEC
                else if ( ch < 0xF4 )
                {
                    if ( ( b2 & 0xC0 ) != 0x80 || ( b3 & 0xC0 ) != 0x80 || ( b4 & 0xC0 ) != 0x80 )
                        throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
                }

                // case for leading byte of exactly 0xF4
                else
                {
                    if ( b2 < 0x80 || b2 > 0x8F || ( b3 & 0xC0 ) != 0x80 || ( b4 & 0xC0 ) != 0x80 )
                        throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
                }

                // compose character
                // valid UTF-32 only
                ch = ( ( ch & 0x07 ) << 18 ) | ( ( b2 & 0x3F ) << 12 ) | ( ( b3 & 0x3F ) << 6 ) | ( b4 & 0x3F );
            }

            // error
            else
            {
                throw InvalidUTF8String ( XP ( XLOC ) << "badly formed UTF-8 character" );
            }
        }

        return UniChar ( ch, len );
    }

    static
    UniChar utf16_to_utf32 ( const UTF16 * data, count_t elems, count_t offset )
    {
        if ( offset >= elems )
            throw BoundsException ( XP ( XLOC ) << "offset leaves no data" );

        UTF16 sur1 = data [ offset ];
        if ( sur1 < 0xD800 || sur1 > 0xDFFF )
            return UniChar ( sur1, 1 );

        if ( ( sur1 & 0xDC00 ) != 0xD800 || offset + 1 == elems )
            throw InvalidUTF16String ( XP ( XLOC ) << "badly formed UTF-16 surrogate pair" );

        UTF16 sur2 = data [ offset + 1 ];
        if ( ( sur2 & 0xDC00 ) != 0xDC00 )
            throw InvalidUTF16String ( XP ( XLOC ) << "badly formed UTF-16 surrogate pair" );

        UTF32 ch = ( ( ( UTF32 ) sur1 - 0xD800 ) << 10 ) + ( sur2 - 0xDC00 ) + 0x10000;
        return UniChar ( ch, 2 );
    }

    static
    UniChar utf16_to_utf32_bswap ( const UTF16 * data, count_t elems, count_t offset )
    {
        if ( offset >= elems )
            throw BoundsException ( XP ( XLOC ) << "offset leaves no data" );

        UTF16 sur1 = bswap_16 ( data [ offset ] );
        if ( sur1 < 0xD800 || sur1 > 0xDFFF )
            return UniChar ( sur1, 1 );

        if ( ( sur1 & 0xDC00 ) != 0xD800 || offset + 1 == elems )
            throw InvalidUTF16String ( XP ( XLOC ) << "badly formed UTF-16 surrogate pair" );

        UTF16 sur2 = bswap_16 ( data [ offset + 1 ] );
        if ( ( sur2 & 0xDC00 ) != 0xDC00 )
            throw InvalidUTF16String ( XP ( XLOC ) << "badly formed UTF-16 surrogate pair" );

        UTF32 ch = ( ( ( UTF32 ) sur1 - 0xD800 ) << 10 ) + ( sur2 - 0xDC00 ) + 0x10000;
        return UniChar ( ch, 2 );
    }

    static
    size_t utf32_to_utf8 ( UTF8 * buff, size_t bsize, UTF32 ch )
    {
        // detect ASCII
        if ( is_ascii ( ch ) )
        {
            if ( bsize == 0 )
            {
                throw InsufficientBuffer (
                    XP ( XLOC )
                    << "buffer size ( "
                    << bsize
                    << " bytes ) insufficient ( "
                    << 1
                    << " needed ) when converting UTF-32 to UTF-8"
                    );
            }

            buff [ 0 ] = static_cast < UTF8 > ( ch );
            return 1;
        }

        // given that this is assumed to be a well-formed UTF-32 character,
        // we should observe the following pattern of leading zeros:
        //
        //   25..32 => single byte ( ASCII )
        //   21..24 => two byte
        //   16..20 => three byte
        //   11..15 => four byte

        unsigned int zeros = __builtin_clz ( ch );
        if ( zeros > 20 )
        {
            // if ch >= 128, there can only be up to 24 zero bits
            // this asserts that our reliance on __builtin_clz() is
            // correct and that it's being used properly.
            assert ( zeros < 25 );

            if ( bsize < 2 )
            {
                throw InsufficientBuffer (
                    XP ( XLOC )
                    << "buffer size ( "
                    << bsize
                    << " bytes ) insufficient ( "
                    << 2
                    << " needed ) when converting UTF-32 to UTF-8"
                    );
            }

            buff [ 0 ] = static_cast < UTF8 > ( ( ch >> 6 ) | 0xC0 );
            buff [ 1 ] = static_cast < UTF8 > ( ( ch & 0x3F ) | 0x80 );
            return 2;
        }
        if ( zeros > 15 )
        {
            if ( bsize < 3 )
            {
                throw InsufficientBuffer (
                    XP ( XLOC )
                    << "buffer size ( "
                    << bsize
                    << " bytes ) insufficient ( "
                    << 3
                    << " needed ) when converting UTF-32 to UTF-8"
                    );
            }

            buff [ 0 ] = static_cast < UTF8 > ( ( ch >> 12 ) | 0xE0 );
            buff [ 1 ] = static_cast < UTF8 > ( ( ( ch >> 6 ) & 0x3F ) | 0x80 );
            buff [ 2 ] = static_cast < UTF8 > ( ( ch & 0x3F ) | 0x80 );
            return 3;
        }
        if ( zeros > 10 )
        {
            if ( bsize < 4 )
            {
                throw InsufficientBuffer (
                    XP ( XLOC )
                    << "buffer size ( "
                    << bsize
                    << " bytes ) insufficient ( "
                    << 4
                    << " needed ) when converting UTF-32 to UTF-8"
                    );
            }

            buff [ 0 ] = static_cast < UTF8 > ( ( ch >> 18 ) | 0xF0 );
            buff [ 1 ] = static_cast < UTF8 > ( ( ( ch >> 12 ) & 0x3F ) | 0x80 );
            buff [ 2 ] = static_cast < UTF8 > ( ( ( ch >> 6 ) & 0x3F ) | 0x80 );
            buff [ 3 ] = static_cast < UTF8 > ( ( ch & 0x3F ) | 0x80 );
            return 4;
        }

        throw InvalidUTF32Character ( XP ( XLOC ) << "badly formed UTF-32 character" );
    }

    static
    size_t utf32_to_utf8_strict ( UTF8 * buff, size_t bsize, UTF32 ch )
    {
        if ( ch >= 0xD800 && ch <= 0xDFFF )
            throw InvalidUTF32Character ( XP ( XLOC ) << "surrogate code points are not included in the set of Unicode scalar values" );
        if ( ch > 0x10FFFF )
            throw InvalidUTF32Character ( XP ( XLOC ) << "badly formed UTF-32 character" );

        // TBD - check for other oddities

        return utf32_to_utf8 ( buff, bsize, ch );
    }


    /*=================================================*
     *                     String                      *
     *=================================================*/

    bool String :: isEmpty () const noexcept
    {
        return str -> s . empty ();
    }

    bool String :: isAscii () const noexcept
    {
        return str -> s . size () == cnt;
    }

    size_t String :: size () const noexcept
    {
        return str -> s . size ();
    }

    const UTF8 * String :: data () const noexcept
    {
        return str -> s . data ();
    }


    UTF32 String :: getChar ( count_t idx ) const
    {
        // in keeping with C/C++ but incompatible with Java,
        // we assume here that idx is a Natural number, and
        // can never be negative. Only check upper bound.
        if ( idx >= cnt )
        {
            throw BoundsException (
                XP ( XLOC )
                << "invalid string index: "
                << idx
                );
        }

        // if self is all ASCII, then STL is sufficient
        if ( isAscii () )
            return str -> s . at ( idx );

        // since we have some UTF-8, find the byte offset to character
        size_t offset = char_idx_to_byte_off ( str -> s . data (), str -> s . size (), idx );

        // transform the character to UTF-32
        return utf8_to_utf32 ( str -> s . data (), str -> s . size (), offset );
    }

    bool String :: equal ( const String & s ) const noexcept
    {
        // first, check that the size in bytes matches
        // and the count in characters.
        if ( size () == s . size () && cnt == s . cnt )
        {
            // at this point, compare the contents
            return memcmp ( data (), s . data (), size () ) == 0;
        }
        return false;
    }

    int String :: compare ( const String & s ) const noexcept
    {
        // if 100% ASCII, STL will do the job
        if ( isAscii () && s . isAscii () )
            return str -> s . compare ( s . str -> s );

        // otherwise, walk each string in UNICODE
        Iterator a = makeIterator ();
        Iterator b = s . makeIterator ();
        for ( ; a . isValid () && b . isValid (); ++ a, ++ b )
        {
            // both iterators are valid
            // retrieve a UTF-32 character at position
            UTF32 ch_a = * a;
            UTF32 ch_b = * b;
            if ( ch_a != ch_b )
            {
                // a mismatch - use encoding for lexical order
                return ( ch_a < ch_b ) ? -1 : 1;
            }
        }

        // at this point, one or the other or both iterators
        // have reached the end of their strings, meaning that
        // all characters before now matched.

        if ( b . isValid () )
        {
            // if "b" is still valid, then "a" must have run to the end
            assert ( ! a . isValid () );

            // "a" is shorter than "b"
            // but otherwise compares identically
            return -1;
        }

        // if "a" is still valid, then it is longer than "b"
        // resulting in a return value of 1.

        // but if both "a" and "b" became invalid at the same time,
        // then the strings compare equally and a value of 0 is returned.

        // more C++ trickery...
        // a bool is a byte with either a 0 or 1 value
        // these are the two values we want to return,
        // so let the compiler promote a bool to an int
        return a . isValid ();
    }

    int String :: caseInsensitiveCompare ( const String & s ) const noexcept
    {
        Iterator a = makeIterator ();
        Iterator b = s . makeIterator ();
        for ( ; a . isValid () && b . isValid (); ++ a, ++ b )
        {
            UTF32 ch_a = * a;
            UTF32 ch_b = * b;
            if ( ch_a != ch_b )
            {
                // if either or both characters are non-alphabetic,
                // case is not an issue in the comparison
                if ( ! iswalpha ( ch_a ) || ! iswalpha ( ch_b ) )
                    return ( ch_a < ch_b ) ? -1 : 1;

                // map to the same case - not important whether upper or lower
                ch_a = towupper ( ch_a );
                ch_b = towupper ( ch_b );
                if ( ch_a != ch_b )
                {
                    // return case-insensitive ordering
                    return ( ch_a < ch_b ) ? -1 : 1;
                }
            }
        }

        // see "compare()" for remaining comments

        if ( b . isValid () )
        {
            assert ( ! a . isValid () );
            return -1;
        }

        return a . isValid ();
    }

    /*==================================================================================*
     *                          HIGHLY REPETITIVE CODE SECTION                          *
     * The code below in the "find()", "rfind()", "findFirstOf()" and "findLastOf()"    *
     * suite contains highly repetitive code that often does not lend itself to         *
     * subroutining; largely because of multiple return paths and access to several     *
     * variables throughout the method.                                                 *
     *                                                                                  *
     * One approach that would work to consolidate code would be to use (gulp) macros,  *
     * because these are not bound by single exit paths because they are not stacked.   *
     *                                                                                  *
     * Another approach is just to repeat the patterns and review them carefully.       *
     *==================================================================================*/


#define DETECT_NULL_ZSTR( zstr )                \
    do {                                        \
        if ( zstr == nullptr )                  \
        {                                       \
            throw NullArgumentException (       \
                XP ( XLOC ) <<                  \
                "null pointer when expecting "  \
                "a NUL-terminated string" );    \
        }                                       \
    } while ( 0 )

#define STRING_FIND_DECLARE_VARS_1( ... )     \
    size_t pos;                               \
    __VA_ARGS__;                              \
    do {                                      \
        if ( len == 0 || left >= cnt )        \
            return npos;                      \
        if ( len > cnt )                      \
            len = cnt;                        \
        xright = left + len;                  \
    } while ( 0 )

#define STRING_FIND_DECLARE_VARS_2( meth, query, ... )        \
    size_t pos;                                               \
    __VA_ARGS__;                                              \
    do {                                                      \
        if ( len == 0 || left >= cnt )                        \
            return npos;                                      \
        if ( ! isAscii () )                                   \
            return meth ( String ( query ), left, len );      \
        if ( ! is_ascii ( query ) )                           \
            return npos;                                      \
        if ( len > cnt )                                      \
            len = cnt;                                        \
        xright = left + len;                                  \
    } while ( 0 )

#define STRING_FIND_DECLARE_VARS_3( meth, query, ... )        \
    size_t pos;                                               \
    __VA_ARGS__;                                              \
    do {                                                      \
        if ( len == 0 || left >= cnt )                        \
            return npos;                                      \
        if ( ! isAscii () || ! is_ascii ( query ) )           \
            return meth ( String ( query ), left, len );      \
        if ( len > cnt )                                      \
            len = cnt;                                        \
        xright = left + len;                                  \
    } while ( 0 )

#define STRING_FIND_ASCII_RETURN_1( meth, str, query, lim )     \
    do {                                                        \
        if ( xright >= cnt )                                    \
            return str -> s . meth ( query, left );             \
        if ( cnt - xright < lim )                               \
        {                                                       \
            pos = str -> s . meth ( query, left );              \
            return ( pos >= xright ) ? npos : pos;              \
        }                                                       \
        pos = str -> s . substr ( left, len ) . meth ( query ); \
        return ( pos == str -> s . npos ) ? npos : pos + left;  \
    } while ( 0 )

#define STRING_FIND_UTF8_RETURN_1( meth, str, query )                    \
    do {                                                                 \
        size_t start = 0;                                                \
        if ( left != 0 )                                                 \
            start = char_idx_to_byte_off (                               \
                str -> s . data (),                                      \
                str -> s . size (),                                      \
                left );                                                  \
        size_t size = str -> s . npos;                                   \
        if ( xright < cnt )                                              \
        {                                                                \
            size = char_idx_to_byte_off (                                \
                str -> s . data () + start,                              \
                str -> s . size () - start,                              \
                len );                                                   \
        }                                                                \
        pos = str -> s . substr ( start, size ) . meth ( query );        \
        if ( pos == str -> s . npos )                                    \
            return npos;                                                 \
        return string_length ( str -> s . data () + start, pos ) + left; \
    } while ( 0 )

#define STRING_RFIND_DECLARE_VARS_1( ... )      \
    size_t pos;                                 \
    __VA_ARGS__;                                \
    do {                                        \
        if ( len == 0 || xright == 0 )          \
            return npos;                        \
        if ( xright > cnt )                     \
            xright = cnt;                       \
        if ( len > xright )                     \
            len = xright;                       \
        left = xright - len;                    \
    } while ( 0 )

#define STRING_RFIND_DECLARE_VARS_2( meth, query, ... )    \
    size_t pos;                                            \
    __VA_ARGS__;                                           \
    do {                                                   \
        if ( len == 0 || xright == 0 )                     \
            return npos;                                   \
        if ( ! isAscii () )                                \
            return meth ( String ( query ), xright, len ); \
        if ( ! is_ascii ( query ) )                        \
            return npos;                                   \
        if ( xright > cnt )                                \
            xright = cnt;                                  \
        if ( len > xright )                                \
            len = xright;                                  \
        left = xright - len;                               \
    } while ( 0 )

#define STRING_RFIND_DECLARE_VARS_3( meth, query, ... )    \
    size_t pos;                                            \
    __VA_ARGS__;                                           \
    do {                                                   \
        if ( len == 0 || xright == 0 )                     \
            return npos;                                   \
        if ( ! isAscii () || ! is_ascii ( query ) )        \
            return meth ( String ( query ), xright, len ); \
        if ( xright > cnt )                                \
            xright = cnt;                                  \
        if ( len > xright )                                \
            len = xright;                                  \
        left = xright - len;                               \
    } while ( 0 )

#define STRING_RFIND_ASCII_RETURN_1( meth, str, query, lim )    \
    do {                                                        \
        if ( left == 0 )                                        \
            return str -> s . meth ( query, xright - 1 );       \
        if ( left < lim )                                       \
        {                                                       \
            pos = str -> s . meth ( query, xright - 1 );        \
            return ( pos < left ) ? npos : pos;                 \
        }                                                       \
        pos = str -> s . substr ( left, len ) . meth ( query ); \
        return ( pos == str -> s . npos ) ? npos : pos + left;  \
    } while ( 0 )

#define STRING_RFIND_UTF8_RETURN_1( meth, str, query )  \
    STRING_FIND_UTF8_RETURN_1 ( meth, str, query )
        

    count_t String :: find ( const String & sub, count_t left, count_t len ) const noexcept
    {
        // the beauty of "len" vs. a right edge coordinate is that
        // it is unambiguous. the problem is that you end up needing
        // to generate a right edge and defining what it means.
        //
        // in our case, we use "xright" to mean it is exclusive of the
        // interval, meaning the interval is [left, xright)
        STRING_FIND_DECLARE_VARS_1 ( count_t xright );

        // STL will be able to find a substring whether in ASCII or UTF-8,
        // provided that the UTF-8 is canonically represented. We special-
        // case when our string is ASCII so that byte offsets can be used
        // as character indices.
        if ( isAscii () )
        {
            // knowing that self is 100% ASCII, ask whether sub is as well
            // if it contains any non-ASCII characters, there is no match
            if ( ! sub . isAscii () )
                return npos;

            // run ASCII within ASCII
            STRING_FIND_ASCII_RETURN_1 ( find, str, sub . str -> s, nsquare_search_edge );
        }

        // character offsets cannot be used. Instead, transform them to
        // byte offsets and back again.
        STRING_FIND_UTF8_RETURN_1 ( find, str, sub . str -> s );
    }

    count_t String :: find ( const UTF8 * zstr, count_t left, count_t len ) const
    {
        // a NULL "zstr" will cause an exception to be thrown
        DETECT_NULL_ZSTR ( zstr );

        // declare variables as above, with a twist in that
        // if either self or "zstr" contain non-ASCII characters,
        // convert "zstr" into a String and invoke the main "find()"
        STRING_FIND_DECLARE_VARS_2 ( find, zstr, count_t xright );

        // at this point, we know that all processing is 100% ASCII
        STRING_FIND_ASCII_RETURN_1 ( find, str, zstr, nsquare_search_edge );
    }

    count_t String :: find ( UTF32 ch, count_t left, count_t len ) const noexcept
    {
        STRING_FIND_DECLARE_VARS_2 ( find, ch, count_t xright );
        STRING_FIND_ASCII_RETURN_1 ( find, str, static_cast < ASCII > ( ch ), simple_search_edge );
    }

    count_t String :: rfind ( const String & sub, count_t xright, count_t len ) const noexcept
    {
        STRING_RFIND_DECLARE_VARS_1 ( count_t left );

        if ( isAscii () )
        {
            if ( ! sub . isAscii () )
                return npos;
            STRING_RFIND_ASCII_RETURN_1 ( rfind, str, sub . str -> s, nsquare_search_edge );
        }

        STRING_RFIND_UTF8_RETURN_1( rfind, str, sub . str -> s );
    }

    count_t String :: rfind ( const UTF8 * zstr, count_t xright, count_t len ) const
    {
        DETECT_NULL_ZSTR ( zstr );
        STRING_RFIND_DECLARE_VARS_2 ( rfind, zstr, count_t left );
        STRING_RFIND_ASCII_RETURN_1 ( rfind, str, zstr, nsquare_search_edge );
    }

    count_t String :: rfind ( UTF32 ch, count_t xright, count_t len ) const noexcept
    {
        STRING_RFIND_DECLARE_VARS_2 ( rfind, ch, count_t left );
        STRING_RFIND_ASCII_RETURN_1 ( rfind, str, static_cast < ASCII > ( ch ), simple_search_edge );
    }

    count_t String :: findFirstOf ( const String & cset, count_t left, count_t len ) const noexcept
    {
        // declare variables as in "find()"
        STRING_FIND_DECLARE_VARS_1 ( count_t xright );

        // if the find set is empty, every character matches
        // return the left-most character index of region
        if ( cset . isEmpty () )
            return left;

        // STL will work properly if self is ASCII
        if ( isAscii () )
            STRING_FIND_ASCII_RETURN_1 ( find_first_of, str, cset . str -> s, nsquare_search_edge );

        // STL will kinda work if cset is ASCII
        // but there will be work to convert pos from bytes to characters
        if ( cset . isAscii () )
            STRING_FIND_UTF8_RETURN_1 ( find_first_of, str, cset . str -> s );

        // "xright" may be beyond the end of the string
        // crop it to the exact end
        if ( xright > cnt )
            xright = cnt;

        // walk across self, one character at a time
        Iterator a = makeIterator ( left );
        for ( count_t i = left; a . isValid () && i < xright; ++ a, ++ i )
        {
            // retrieve current character
            UTF32 ch_a = * a;

            // walk across set
            Iterator b = cset . makeIterator ();
            for ( ; b . isValid (); ++ b )
            {
                // return a match at this character index
                if ( ch_a == * b )
                    return a . idx;
            }
        }

        return npos;
    }

    count_t String :: findFirstOf ( const UTF8 * zcset, count_t left, count_t len ) const
    {
        DETECT_NULL_ZSTR ( zcset );
        STRING_FIND_DECLARE_VARS_3 ( findFirstOf, zcset, count_t xright );

        if ( zcset [ 0 ] == 0 )
            return left;

        STRING_FIND_ASCII_RETURN_1 ( find_first_of, str, zcset, nsquare_search_edge );
    }

    count_t String :: findLastOf ( const String & cset, count_t xright, count_t len ) const noexcept
    {
        STRING_RFIND_DECLARE_VARS_1 ( count_t left );

        if ( cset . isEmpty () )
            return xright - 1;

        if ( isAscii () )
            STRING_RFIND_ASCII_RETURN_1 ( find_last_of, str, cset . str -> s, nsquare_search_edge );

        if ( cset . isAscii () )
            STRING_RFIND_UTF8_RETURN_1 ( find_last_of, str, cset . str -> s );

        // walk across self, one character at a time
        Iterator a = makeIterator ( xright - 1 );
        for ( count_t i = xright; a . isValid () && i > left; -- a, -- i )
        {
            // retrieve current character
            UTF32 ch_a = * a;

            // walk across set
            Iterator b = cset . makeIterator ();
            for ( ; b . isValid (); ++ b )
            {
                // return a match at this character index
                if ( ch_a == * b )
                    return a . idx;
            }
        }

        return npos;
    }

    count_t String :: findLastOf ( const UTF8 * zcset, count_t xright, count_t len ) const
    {
        DETECT_NULL_ZSTR ( zcset );
        STRING_RFIND_DECLARE_VARS_3 ( findLastOf, zcset, count_t left );

        if ( zcset [ 0 ] == 0 )
            return xright - 1;

        STRING_RFIND_ASCII_RETURN_1 ( find_last_of, str, zcset, nsquare_search_edge );
    }

    bool String :: beginsWith ( const String & sub ) const noexcept
    {
        // if the sizes are identical
        if ( cnt == sub . cnt )
            return equal ( sub );

        // if self could possibly contain sub
        if ( cnt > sub . cnt )
            return find ( sub, 0, sub . cnt ) == 0;

        // cannot begin with sub
        return false;
    }

    bool String :: beginsWith ( const UTF8 * zsub ) const
    {
        DETECT_NULL_ZSTR ( zsub );
        if ( isAscii () && is_ascii ( zsub ) )
        {
            count_t i;
            const ASCII * data = str -> s . data ();
            for ( i = 0; i < cnt; ++ i )
            {
                if ( zsub [ i ] == 0 )
                    return true;
                if ( data [ i ] != zsub [ i ] )
                    return false;
            }
            return zsub [ i ] == 0;
        }

        return beginsWith ( String ( zsub ) );
    }

    bool String :: beginsWith ( UTF32 ch ) const
    {
        if ( cnt == 0 )
            return false;
        if ( getChar ( 0 ) == ch )
            return true;
        if ( ! isAscii () && ! is_ascii ( ch ) )
            return beginsWith ( String ( ch ) );
        return false;
    }

    bool String :: endsWith ( const String & sub ) const noexcept
    {
        // if the sizes are identical
        if ( cnt == sub . cnt )
            return equal ( sub );

        // if self could possibly contain sub
        if ( cnt > sub . cnt )
            return find ( sub, cnt - sub . cnt ) != npos;

        // cannot end with sub
        return false;
    }

    bool String :: endsWith ( const UTF8 * zsub ) const
    {
        DETECT_NULL_ZSTR ( zsub );
        return endsWith ( String ( zsub ) );
    }

    bool String :: endsWith ( UTF32 ch ) const
    {
        if ( cnt == 0 )
            return false;
        if ( getChar ( cnt - 1 ) == ch )
            return true;
        if ( ! isAscii () && ! is_ascii ( ch ) )
            return endsWith ( String ( ch ) );
        return false;
    }

    String String :: subString ( count_t left, count_t len ) const
    {
        // create an empty string to return
        String sub;

        if ( left < cnt && len != 0 )
        {
            if ( isAscii () )
            {
                //sub . str = new wipersnapper ( * str, left, len );
                sub . str = new Wiper ( * str, left, len );
                sub . cnt = sub . str -> s . size ();
            }
            else
            {
                if ( len > cnt )
                    len = cnt;

                if ( left + len > cnt )
                    len = cnt - left;

                size_t start = 0;
                if ( left != 0 )
                    start = char_idx_to_byte_off ( str -> s . data (), str -> s . size (), left );

                size_t size = str -> s . npos;
                if ( left + len < cnt )
                {
                    size = char_idx_to_byte_off (
                        str -> s . data () + start,
                        str -> s . size () - start,
                        len );
                }

                sub . str = new Wiper ( * str, start, size );
                sub . cnt = len;
            }
        }

        return sub;
    }

    String String :: concat ( const String & s ) const
    {
        // copy current contents
        String cpy ( * this );

        // append "s"
        cpy . str -> append ( * s . str );
        cpy . cnt += s . cnt;

        return cpy;
    }

    String String :: concat ( const UTF8 * zstr ) const
    {
        if ( zstr == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "null pointer when expecting a NUL-terminated string" );

        // don't bother processing non-ASCII here
        if ( ! is_ascii ( zstr ) )
            return concat ( String ( zstr ) );

        // ASCII concat
        String cpy ( * this );
        cpy . str -> s . append ( zstr );
        cpy . cnt += string_size ( zstr );
        return cpy;
    }

    String String :: concat ( UTF32 ch ) const
    {
        // detect non-ASCII
        if ( ! is_ascii ( ch ) )
        {
            UTF8 buff [ 8 ];
            size_t len = utf32_to_utf8_strict ( buff, sizeof buff, ch );
            return concat ( String ( buff, len ) );
        }

        // add single ASCII character to end
        String cpy ( * this );
        cpy . str -> s . push_back ( static_cast < ASCII > ( ch ) );
        cpy . cnt += 1;
        return cpy;
    }

    String String :: toupper () const
    {
        StringBuffer sb;
        sb += * this;
        return sb . toupper () . stealString ();
    }

    String String :: tolower () const
    {
        StringBuffer sb;
        sb += * this;
        return sb . tolower () . stealString ();
    }

    void String :: clear ( bool _wipe ) noexcept
    {
        // do NOT optimize this into str->wipe |= _wipe
        if ( _wipe )
            str -> wipe = true;
        else
            _wipe = str -> wipe;

        str = new Wiper ( _wipe );
        cnt = 0;
    }

    std :: string String :: toSTLString () const
    {
        return std :: string ( str -> s );
    }

    String :: Iterator String :: makeIterator ( count_t origin ) const noexcept
    {
        // incoming origin is in characters
        // we need it in byte offset form
        size_t offset = origin;
        if ( origin != 0 )
        {
            // detect end of string
            if ( origin >= cnt )
            {
                offset = str -> s . size ();
                origin = cnt;
            }

            // if self contains multi-byte characters,
            // map the character index to a byte offset
            else if ( ! isAscii () )
            {
                offset = char_idx_to_byte_off ( str -> s . data (), str -> s . size (), origin );
            }
        }

        // create an iterator with a reference to self
        // this is potentially dangerous, so we maintain
        // outstanding iterators in a linked list.
        return Iterator ( str, cnt, offset, origin );
    }


    String & String :: operator = ( const String & s )
    {
        str = s . str;
        cnt = s . cnt;
        return * this;
    }

    String :: String ( const String & s )
        : str ( s . str )
        , cnt ( s . cnt )
    {
    }

    String & String :: operator = ( const String && s )
    {
        str = std :: move ( s . str );
        cnt = s . cnt;
        return * this;
    }

    String :: String ( const String && s )
        : str ( std :: move ( s . str ) )
        , cnt ( s . cnt )
    {
    }

    String :: String ()
        : str ( new Wiper )
        , cnt ( 0 )
    {
    }

    String :: String ( UTF32 ch )
        : cnt ( 0 )
    {
        if ( is_ascii ( ch ) )
        {
            str = new Wiper;
            str -> s += ( char ) ch;
            cnt = 1;
        }
        else
        {
            UTF8 buffer [ 16 ];
            size_t bytes = utf32_to_utf8_strict ( buffer, sizeof buffer, ch );
            cnt = unicode ( buffer, bytes );
        }
    }

    String :: String ( const UTF8 * zstr )
        : cnt ( 0 )
    {
        // sorry about the exception
        if ( zstr == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "null pointer when expecting a NUL-terminated string" );

        // 100% ASCII is fine
        if ( is_ascii ( zstr ) )
        {
            str = new Wiper ( zstr, false );
            cnt = str -> s . size ();
        }
        else
        {
            // need to perform normalization to canonical form
            cnt = unicode ( zstr, string_size ( zstr ) );
        }
    }

    String :: String ( const UTF8 * s, size_t bytes )
        : cnt ( 0 )
    {
        if ( s == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "null pointer when expecting a string buffer pointer" );


        if ( is_ascii ( s, bytes ) )
        {
            str = new Wiper ( s, bytes, false );
            cnt = str -> s . size ();
        }
        else
        {
            cnt = unicode ( s, bytes );
        }
    }

    String :: String ( const UTF16 * zstr )
        : cnt ( 0 )
    {
        // sorry about the exception
        if ( zstr == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "null pointer when expecting a NUL-terminated string" );

        cnt = unicode ( zstr, string_elems ( zstr ) );
    }

    String :: String ( const UTF16 * s, count_t elems )
        : cnt ( 0 )
    {
        if ( s == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "null pointer when expecting a string buffer pointer" );

        cnt = unicode ( s, elems );
    }

    String :: String ( const UTF32 * zstr )
        : cnt ( 0 )
    {
        // sorry about the exception
        if ( zstr == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "null pointer when expecting a NUL-terminated string" );

        cnt = unicode ( zstr, string_elems ( zstr ) );
    }

    String :: String ( const UTF32 * s, count_t elems )
        : cnt ( 0 )
    {
        if ( s == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "null pointer when expecting a string buffer pointer" );

        cnt = unicode ( s, elems );
    }

    String :: String ( const std :: string & s )
        : cnt ( 0 )
    {
        if ( is_ascii ( s . data (), s . size () ) )
        {
            str = new Wiper ( s, false );
            cnt = str -> s . size ();
        }
        else
        {
            cnt = unicode ( s . data (), s . size () );
        }
    }

    String :: ~ String ()
    {
    }

    count_t String :: unicode ( const UTF8 * s, size_t elems )
    {
        // detect BOM - useless, but legal.
        assert ( s != nullptr );
        if ( elems >= 3 )
        {
            const unsigned char * us = ( const unsigned char * ) s;
            if ( us [ 0 ] == 0xEF && us [ 1 ] == 0xBB && us [ 2 ] == 0xBF )
            {
                s += 3;
                elems -= 3;
            }
        }

        // count the number of UTF-8 characters
        count_t len = string_length ( s, elems );

        // create a UTF-32 buffer
        UTF32 * utf32;
        Payload buffer ( ( len + 1 ) * sizeof * utf32 );
        utf32 = ( UTF32 * ) buffer . data ();

        // convert UTF-8 to UTF-32 using strict checking
        count_t i, offset = 0;
        for ( i = 0; i < len; ++ i )
        {
            UniChar uch = utf8_to_utf32_strict ( s, elems, offset );
            utf32 [ i ] = uch . ch;
            offset += uch . elems;
        }

        // finishing touches on buffer for debugging and correctness
        utf32 [ i ] = 0;
        buffer . setSize ( i * sizeof * utf32 );

        // use buffer memory to normalize UTF-32 string
        // and assign it into the UTF-8 data member
        return normalize ( utf32, i );
    }

    count_t String :: unicode ( const UTF16 * s, size_t elems )
    {
        // BOM - not expected, but must be handled
        assert ( s != nullptr );
        bool bswap = false;
        if ( elems >= 1 )
        {
            if ( s [ 0 ] == 0xFEFF )
            {
                ++ s;
                -- elems;
            }
            else if ( s [ 0 ] == 0xFFFE )
            {
                bswap = true;
                ++ s;
                -- elems;
            }
        }

        // count the number of UTF-16 characters
        count_t len = bswap ?
            string_length_bswap ( s, elems ):
            string_length ( s, elems );

        // create a UTF-32 buffer
        UTF32 * utf32;
        Payload buffer ( ( len + 1 ) * sizeof * utf32 );
        utf32 = ( UTF32 * ) buffer . data ();

        // convert UTF-16 to UTF-32 using strict checking
        count_t i, offset = 0;
        if ( bswap )
        {
            for ( i = 0; i < len; ++ i )
            {
                UniChar uch = utf16_to_utf32_bswap ( s, elems, offset );
                utf32 [ i ] = uch . ch;
                offset += uch . elems;
            }
        }
        else
        {
            for ( i = 0; i < len; ++ i )
            {
                UniChar uch = utf16_to_utf32 ( s, elems, offset );
                utf32 [ i ] = uch . ch;
                offset += uch . elems;
            }
        }

        // finishing touches on buffer for debugging and correctness
        utf32 [ i ] = 0;
        buffer . setSize ( i * sizeof * utf32 );

        // use buffer memory to normalize UTF-32 string
        // and assign it into the UTF-8 data member
        return normalize ( utf32, i );
    }

    count_t String :: unicode ( const UTF32 * s, size_t elems )
    {
        // BOM - not expected, but must be handled
        assert ( s != nullptr );
        bool bswap = false;
        if ( elems >= 1 )
        {
            if ( s [ 0 ] == 0xFEFF )
            {
                ++ s;
                -- elems;
            }
            else if ( s [ 0 ] == 0xFFFE )
            {
                bswap = true;
                ++ s;
                -- elems;
            }
        }

        // count the number of UTF-32 characters
        count_t len = elems;

        // create a UTF-32 buffer
        UTF32 * utf32;
        Payload buffer ( ( len + 1 ) * sizeof * utf32 );
        utf32 = ( UTF32 * ) buffer . data ();

        // copy UTF-32 characters to buffer
        count_t i;
        if ( bswap )
        {
            for ( i = 0; i < len; ++ i )
                utf32 [ i ] = bswap_32 ( s [ i ] );
        }
        else
        {
            for ( i = 0; i < len; ++ i )
                utf32 [ i ] = s [ i ];
        }

        // finishing touches on buffer for debugging and correctness
        utf32 [ i ] = 0;
        buffer . setSize ( i * sizeof * utf32 );

        // use buffer memory to normalize UTF-32 string
        // and assign it into the UTF-8 data member
        return normalize ( utf32, i );
    }

    count_t String :: normalize ( UTF32 * buffer, count_t ccnt )
    {
        // use external library to normalize in place
        utf8proc_int32_t * b = reinterpret_cast < utf8proc_int32_t * > ( buffer );
        utf8proc_ssize_t ncnt = utf8proc_normalize_utf32 ( b, ccnt, UTF8PROC_COMPOSE );
        if ( ncnt < 0 )
        {
            switch ( ncnt )
            {
            case UTF8PROC_ERROR_NOMEM:
                throw MemoryExhausted (
                    XP ( XLOC )
                    << "utf8proc_normalize_utf32 - "
                    << utf8proc_errmsg ( ncnt )
                    );
            case UTF8PROC_ERROR_OVERFLOW:
                throw BoundsException (
                    XP ( XLOC )
                    << "utf8proc_normalize_utf32 - "
                    << utf8proc_errmsg ( ncnt )
                    );
            case UTF8PROC_ERROR_INVALIDUTF8:
            case UTF8PROC_ERROR_NOTASSIGNED:
            case UTF8PROC_ERROR_INVALIDOPTS:
                throw InvalidUTF32Character (
                    XP ( XLOC )
                    << "utf8proc_normalize_utf32 - "
                    << utf8proc_errmsg ( ncnt )
                    );
            default:
                throw InvalidArgument (
                    XP ( XLOC )
                    << "utf8proc_normalize_utf32 - "
                    << utf8proc_errmsg ( ncnt )
                    );
            }
        }

        // we can now transform to UTF-8 in place, given the restriction that
        // UTF-8 uses 1..4 bytes per character while UTF-32 uses 4 bytes, so
        // it will work in order.
        const UTF32 * utf32 = buffer;
        UTF8 * utf8 = reinterpret_cast < UTF8 * > ( buffer );

        count_t i, offset;
        for ( i = 0, ccnt = ncnt, offset = 0; i < ccnt; ++ i )
        {
            // if our code disagrees with libutf8proc, we could see exceptions
            size_t sz = utf32_to_utf8_strict ( & utf8 [ offset ], sizeof ( UTF32 ), utf32 [ i ] );
            offset += sz;
        }

        // update the string
        str = new Wiper ( utf8, offset, false );

        // return the character count
        return ccnt;
    }

    void String :: Wiper :: append ( const Wiper & str )
    {
        s . append ( str . s );
        if ( str . wipe )
            wipe = true;
    }

    String :: Wiper :: Wiper ()
        : wipe ( false )
    {
    }

    String :: Wiper :: Wiper ( bool _wipe )
        : wipe ( _wipe )
    {
    }

    String :: Wiper :: Wiper ( const UTF8 * zstr, bool _wipe )
        : s ( zstr )
        , wipe ( _wipe )
    {
    }

    String :: Wiper :: Wiper ( const UTF8 * str, size_t bytes, bool _wipe )
        : s ( str, bytes )
        , wipe ( _wipe )
    {
    }

    String :: Wiper :: Wiper ( const std :: string & str, bool _wipe )
        : s ( str )
        , wipe ( _wipe )
    {
    }

    String :: Wiper :: Wiper ( const String :: Wiper & str, size_t pos, size_t bytes )
        : s ( str . s . substr ( pos, bytes ) )
        , wipe ( str . wipe )
    {
    }

    String :: Wiper :: ~ Wiper ()
    {
        if ( wipe )
        {
            size_t bytes = s . capacity ();
            memset_while_respecting_language_semantics
                ( ( void * ) s . data (), bytes, 0, bytes, s . c_str () );
        }
    }

    bool String :: Iterator :: isValid () const noexcept
    {
        if ( str != nullptr )
            return off >= 0 && ( size_t ) off < str -> s . size ();

        return false;
    }

    bool String :: Iterator :: isAscii () const noexcept
    {
        return str -> s . size () == cnt;
    }

    UTF32 String :: Iterator :: get () const
    {
        if ( str == nullptr || off < 0 || ( size_t ) off >= str -> s . size () )
            throw BoundsException ( XP ( XLOC ) << "invalid string index" );

        return utf8_to_utf32 ( str -> s . data (), str -> s . size (), off );
    }


    /*==================================================================================*
     *                          HIGHLY REPETITIVE CODE SECTION                          *
     * The code below in the "find()", "rfind()", "findFirstOf()" and "findLastOf()"    *
     * suite contains highly repetitive code that often does not lend itself to         *
     * subroutining; largely because of multiple return paths and access to several     *
     * variables throughout the method.                                                 *
     *                                                                                  *
     * One approach that would work to consolidate code would be to use (gulp) macros,  *
     * because these are not bound by single exit paths because they are not stacked.   *
     *                                                                                  *
     * Another approach is just to repeat the patterns and review them carefully.       *
     *==================================================================================*/

#define DETECT_MISMATCHED_STRINGS( end )                    \
    do {                                                    \
        if ( str == nullptr || end . str == nullptr )       \
            return false;                                   \
        if ( str != end . str || cnt != end . cnt )         \
        {                                                   \
            throw IteratorsDiffer (                         \
                XP ( XLOC ) <<                              \
                "iterators refer to different Strings" );   \
        }                                                   \
    } while ( 0 )

#define STRING_IT_FIND_CALC_VARS_1()          \
    do {                                      \
        if ( idx < 0 )                        \
        {                                     \
            if ( ( count_t ) - idx > len )    \
                return false;                 \
            len += idx;                       \
            left = 0;                         \
        }                                     \
        if ( len > cnt )                      \
            len = cnt;                        \
        xright = left + len;                  \
    } while ( 0 )

#define STRING_IT_FIND_DECLARE_VARS_1( ... )      \
    size_t pos;                                   \
    __VA_ARGS__;                                  \
    do {                                          \
        if ( str == nullptr )                     \
            return false;                         \
        if ( len == 0 || ( count_t ) idx >= cnt ) \
            return false;                         \
        left = idx;                               \
        STRING_IT_FIND_CALC_VARS_1 ();            \
    } while ( 0 )

#define STRING_IT_FIND_CALC_VARS_2()          \
    do {                                      \
        if ( idx < 0 )                        \
            left = 0;                         \
        if ( xright > cnt )                   \
            xright = cnt;                     \
        assert ( left <= xright );            \
    } while ( 0 )

#define STRING_IT_FIND_DECLARE_VARS_2( ... )             \
    size_t pos;                                          \
    __VA_ARGS__;                                         \
    do {                                                 \
        DETECT_MISMATCHED_STRINGS ( xend );              \
        if ( idx >= xend . idx )                         \
            return false;                                \
        if ( xend . idx <= 0 || ( count_t ) idx >= cnt ) \
            return false;                                \
        left = idx;                                      \
        xright = xend . idx;                             \
        STRING_IT_FIND_CALC_VARS_2 ();                   \
    } while ( 0 )
    
#define STRING_IT_FIND_DECLARE_VARS_3( meth, query, ... )    \
    size_t pos;                                              \
    __VA_ARGS__;                                             \
    do {                                                     \
        if ( str == nullptr )                                \
            return false;                                    \
        if ( len == 0 || ( count_t ) idx >= cnt )            \
            return false;                                    \
        if ( ! isAscii () || ! is_ascii ( query ) )          \
            return meth ( String ( query ), len );           \
        left = idx;                                          \
        STRING_IT_FIND_CALC_VARS_1 ();                       \
    } while ( 0 )
    
#define STRING_IT_FIND_DECLARE_VARS_4( meth, query, ... )    \
    size_t pos;                                              \
    __VA_ARGS__;                                             \
    do {                                                     \
        DETECT_MISMATCHED_STRINGS ( xend );                  \
        if ( idx >= xend . idx )                             \
            return false;                                    \
        if ( xend . idx <= 0 || ( count_t ) idx >= cnt )     \
            return false;                                    \
        if ( ! isAscii () || ! is_ascii ( query ) )          \
            return meth ( String ( query ), xend );          \
        left = idx;                                          \
        xright = xend . idx;                                 \
        STRING_IT_FIND_CALC_VARS_2 ();                       \
    } while ( 0 )

#define STRING_IT_FIND_ASCII_RETURN_1( meth, str, query, lim )            \
    do {                                                                  \
        if ( xright >= cnt || cnt - xright < lim )                        \
        {                                                                 \
            pos = str -> s . meth ( query, left );                        \
            if ( pos >= xright )                                          \
                return false;                                             \
            off = idx = pos;                                              \
            return true;                                                  \
        }                                                                 \
        pos = str -> s . substr ( left, xright - left ) . meth ( query ); \
        if ( pos == str -> s . npos )                                     \
            return false;                                                 \
        off = idx = pos + left;                                           \
        return true;                                                      \
    } while ( 0 )

#define STRING_IT_FIND_UTF8_RETURN_1( meth, str, query )                \
    do {                                                                \
        size_t start = off;                                             \
        if ( idx < 0 )                                                  \
            start = 0;                                                  \
        size_t size = str -> s . npos;                                  \
        if ( xright < cnt )                                             \
        {                                                               \
            assert ( left + len == xright );                            \
            size = char_idx_to_byte_off (                               \
                str -> s . data () + start,                             \
                str -> s . size () - start,                             \
                len );                                                  \
        }                                                               \
        pos = str -> s . substr ( start, size ) . meth ( query );       \
        if ( pos == str -> s . npos )                                   \
            return false;                                               \
        off = start + pos;                                              \
        idx = string_length ( str -> s . data () + start, pos ) + left; \
        return true;                                                    \
    } while ( 0 )

#define STRING_IT_FIND_UTF8_RETURN_2( meth, str, query )                \
    do {                                                                \
        size_t start = off;                                             \
        if ( idx < 0 )                                                  \
            start = 0;                                                  \
        assert ( ( size_t )  xend . off > start );                      \
        size_t stop = xend . off;                                       \
        size_t size = stop - start;                                     \
        pos = str -> s . substr ( start, size ) . meth ( query );       \
        if ( pos == str -> s . npos )                                   \
            return false;                                               \
        off = start + pos;                                              \
        idx = string_length ( str -> s . data () + start, pos ) + left; \
        return true;                                                    \
    } while ( 0 )

#define STRING_IT_RFIND_CALC_VARS_1()         \
    do {                                      \
        if ( xright > cnt )                   \
            xright = cnt;                     \
        if ( len > xright )                   \
            len = xright;                     \
        left = xright - len;                  \
    } while ( 0 )

#define STRING_IT_RFIND_DECLARE_VARS_1( ... ) \
    size_t pos;                               \
    __VA_ARGS__;                              \
    do {                                      \
        if ( str == nullptr )                 \
            return false;                     \
        if ( len == 0 || idx <= 0 )           \
            return false;                     \
        xright = idx;                         \
        STRING_IT_RFIND_CALC_VARS_1();        \
    } while ( 0 )

#define STRING_IT_RFIND_CALC_VARS_2()         \
    do {                                      \
        if ( end . idx < 0 )                  \
            left = 0;                         \
        if ( xright > cnt )                   \
            xright = cnt;                     \
        assert ( left <= xright );            \
    } while ( 0 )

#define STRING_IT_RFIND_DECLARE_VARS_2( ... )           \
    size_t pos;                                         \
    __VA_ARGS__;                                        \
    do {                                                \
        DETECT_MISMATCHED_STRINGS ( end );              \
        if ( end . idx >= idx )                         \
            return false;                               \
        if ( idx <= 0 || ( count_t ) end . idx >= cnt ) \
            return false;                               \
        left = end . idx;                               \
        xright = idx;                                   \
        STRING_IT_RFIND_CALC_VARS_2();                  \
    } while ( 0 )
    
#define STRING_IT_RFIND_DECLARE_VARS_3( meth, query, ... )   \
    size_t pos;                                              \
    __VA_ARGS__;                                             \
    do {                                                     \
        if ( str == nullptr )                                \
            return false;                                    \
        if ( len == 0 || idx <= 0 )                          \
            return false;                                    \
        if ( ! isAscii () || ! is_ascii ( query ) )          \
            return meth ( String ( query ), len );           \
        xright = idx;                                        \
        STRING_IT_RFIND_CALC_VARS_1();                       \
    } while ( 0 )
    
#define STRING_IT_RFIND_DECLARE_VARS_4( meth, query, ... )   \
    size_t pos;                                              \
    __VA_ARGS__;                                             \
    do {                                                     \
        DETECT_MISMATCHED_STRINGS ( end );                   \
        if ( end . idx >= idx )                              \
            return false;                                    \
        if ( idx <= 0 || ( count_t ) end . idx >= cnt )      \
            return false;                                    \
        if ( ! isAscii () || ! is_ascii ( query ) )          \
            return meth ( String ( query ), end );           \
        left = end . idx;                                    \
        xright = idx;                                        \
        STRING_IT_RFIND_CALC_VARS_2();                       \
    } while ( 0 )

#define STRING_IT_RFIND_ASCII_RETURN_1( meth, str, query, lim )           \
    do {                                                                  \
        if ( left < lim )                                                 \
        {                                                                 \
            pos = str -> s . meth ( query, xright - 1 );                  \
            if ( pos < left || pos >= xright )                            \
                return false;                                             \
            off = idx = pos;                                              \
            return true;                                                  \
        }                                                                 \
        pos = str -> s . substr ( left, xright - left ) . meth ( query ); \
        if ( pos == str -> s . npos )                                     \
            return false;                                                 \
        off = idx = pos + left;                                           \
        return true;                                                      \
    } while ( 0 )

#define STRING_IT_RFIND_UTF8_RETURN_1( meth, str, query )               \
    do {                                                                \
        size_t start = 0;                                               \
        if ( left != 0 )                                                \
        {                                                               \
            start = char_idx_to_byte_off (                              \
                str -> s . data (),                                     \
                str -> s . size (),                                     \
                left );                                                 \
        }                                                               \
        assert ( ( size_t ) off > start );                              \
        size_t stop = off;                                              \
        size_t size = stop - start;                                     \
        pos = str -> s . substr ( start, size ) . meth ( query );       \
        if ( pos == str -> s . npos )                                   \
            return false;                                               \
        off = start + pos;                                              \
        idx = string_length ( str -> s . data () + start, pos ) + left; \
        return true;                                                    \
    } while ( 0 )

#define STRING_IT_RFIND_UTF8_RETURN_2( meth, str, query )               \
    do {                                                                \
        size_t start = end . off;                                       \
        if ( end . idx < 0 )                                            \
            start = 0;                                                  \
        assert ( ( size_t )  off > start );                             \
        size_t stop = off;                                              \
        size_t size = stop - start;                                     \
        pos = str -> s . substr ( start, size ) . meth ( query );       \
        if ( pos == str -> s . npos )                                   \
            return false;                                               \
        off = start + pos;                                              \
        idx = string_length ( str -> s . data () + start, pos ) + left; \
        return true;                                                    \
    } while ( 0 )


    bool String :: Iterator :: find ( const String & sub, count_t len )
    {
        STRING_IT_FIND_DECLARE_VARS_1 ( count_t left, xright );

        // ASCII optimization
        if ( isAscii () )
        {
            if ( ! sub . isAscii () )
                return false;
            STRING_IT_FIND_ASCII_RETURN_1 ( find, str, sub . str -> s, nsquare_search_edge );
        }

        STRING_IT_FIND_UTF8_RETURN_1 ( find, str, sub . str -> s );
    }

    bool String :: Iterator :: find ( const String & sub, const Iterator & xend )
    {
        STRING_IT_FIND_DECLARE_VARS_2 ( count_t left, xright );

        // ASCII optimization
        if ( isAscii () )
        {
            if ( ! sub . isAscii () )
                return false;
            STRING_IT_FIND_ASCII_RETURN_1 ( find, str, sub . str -> s, nsquare_search_edge );
        }

        STRING_IT_FIND_UTF8_RETURN_2 ( find, str, sub . str -> s );
    }

    bool String :: Iterator :: find ( const UTF8 * zstr, count_t len )
    {
        DETECT_NULL_ZSTR ( zstr );
        STRING_IT_FIND_DECLARE_VARS_3 ( find, zstr, count_t left, xright );
        STRING_IT_FIND_ASCII_RETURN_1 ( find, str, zstr, nsquare_search_edge );
    }

    bool String :: Iterator :: find ( const UTF8 * zstr, const Iterator & xend )
    {
        DETECT_NULL_ZSTR ( zstr );
        STRING_IT_FIND_DECLARE_VARS_4 ( find, zstr, count_t left, xright );
        STRING_IT_FIND_ASCII_RETURN_1 ( find, str, zstr, nsquare_search_edge );
    }

    bool String :: Iterator :: find ( UTF32 ch, count_t len )
    {
        STRING_IT_FIND_DECLARE_VARS_3 ( find, ch, count_t left, xright );
        STRING_IT_FIND_ASCII_RETURN_1 ( find, str, static_cast < ASCII > ( ch ), nsquare_search_edge );
    }

    bool String :: Iterator :: find ( UTF32 ch, const Iterator & xend )
    {
        STRING_IT_FIND_DECLARE_VARS_4 ( find, ch, count_t left, xright );
        STRING_IT_FIND_ASCII_RETURN_1 ( find, str, static_cast < ASCII > ( ch ), nsquare_search_edge );
    }

    bool String :: Iterator :: rfind ( const String & sub, count_t len )
    {
        STRING_IT_RFIND_DECLARE_VARS_1 ( count_t left, xright );

        // ASCII optimization
        if ( isAscii () )
        {
            if ( ! sub . isAscii () )
                return false;
            STRING_IT_RFIND_ASCII_RETURN_1 ( rfind, str, sub . str -> s, nsquare_search_edge );
        }

        STRING_IT_RFIND_UTF8_RETURN_1 ( rfind, str, sub . str -> s );
    }

    bool String :: Iterator :: rfind ( const String & sub, const Iterator & end )
    {
        STRING_IT_RFIND_DECLARE_VARS_2 ( count_t left, xright );

        // ASCII optimization
        if ( isAscii () )
        {
            if ( ! sub . isAscii () )
                return false;
            STRING_IT_RFIND_ASCII_RETURN_1 ( rfind, str, sub . str -> s, nsquare_search_edge );
        }

        STRING_IT_RFIND_UTF8_RETURN_2 ( rfind, str, sub . str -> s );
    }

    bool String :: Iterator :: rfind ( const UTF8 * zstr, count_t len )
    {
        DETECT_NULL_ZSTR ( zstr );
        STRING_IT_RFIND_DECLARE_VARS_3 ( rfind, zstr, count_t left, xright );
        STRING_IT_RFIND_ASCII_RETURN_1 ( rfind, str, zstr, nsquare_search_edge );
    }

    bool String :: Iterator :: rfind ( const UTF8 * zstr, const Iterator & end )
    {
        DETECT_NULL_ZSTR ( zstr );
        STRING_IT_RFIND_DECLARE_VARS_4 ( rfind, zstr, count_t left, xright );
        STRING_IT_RFIND_ASCII_RETURN_1 ( rfind, str, zstr, nsquare_search_edge );
    }

    bool String :: Iterator :: rfind ( UTF32 ch, count_t len )
    {
        STRING_IT_RFIND_DECLARE_VARS_3 ( rfind, ch, count_t left, xright );
        STRING_IT_RFIND_ASCII_RETURN_1 ( rfind, str, static_cast < ASCII > ( ch ), simple_search_edge );
    }

    bool String :: Iterator :: rfind ( UTF32 ch, const Iterator & end )
    {
        STRING_IT_RFIND_DECLARE_VARS_4 ( rfind, ch, count_t left, xright );
        STRING_IT_RFIND_ASCII_RETURN_1 ( rfind, str, static_cast < ASCII > ( ch ), simple_search_edge );
    }

    bool String :: Iterator :: findFirstOf ( const String & cset, count_t len )
    {
        STRING_IT_FIND_DECLARE_VARS_1 ( count_t left, xright );

        // if the find set is empty, current or first legal character matches
        if ( cset . isEmpty () )
        {
            // detect illegal position
            if ( idx < 0 )
            {
                // find the first character
                idx = 0;
                off = 0;
            }
            assert ( ( count_t ) idx < cnt );
            return true;
        }

        // STL will work properly if self is ASCII
        if ( isAscii () )
            STRING_IT_FIND_ASCII_RETURN_1 ( find_first_of, str, cset . str -> s, nsquare_search_edge );

        // STL will kinda work if cset is ASCII
        // but there will be work to convert pos from bytes to characters
        if ( cset . isAscii () )
            STRING_IT_FIND_UTF8_RETURN_1 ( find_first_of, str, cset . str -> s );

        // get an actual length
        if ( xright > cnt )
            xright = cnt;

        // walk across string, one character at a time
        long int save_idx = idx;
        long int save_off = off;
        Iterator & a = * this;

        for ( count_t i = left; a . isValid () && i < xright; ++ a, ++ i )
        {
            // retrieve current character
            UTF32 ch_a = * a;

            // walk across set
            Iterator b = cset . makeIterator ();
            for ( ; b . isValid (); ++ b )
            {
                // return a match at this character index
                if ( ch_a == * b )
                    return true;
            }
        }

        idx = save_idx;
        off = save_off;

        return false;
    }

    bool String :: Iterator :: findFirstOf ( const String & cset, const Iterator & xend )
    {
        STRING_IT_FIND_DECLARE_VARS_2 ( count_t left, xright );

        if ( cset . isEmpty () )
        {
            if ( idx < 0 )
            {
                idx = 0;
                off = 0;
            }
            assert ( ( count_t ) idx < cnt );
            return true;
        }

        if ( isAscii () )
            STRING_IT_FIND_ASCII_RETURN_1 ( find_first_of, str, cset . str -> s, nsquare_search_edge );

        if ( cset . isAscii () )
            STRING_IT_FIND_UTF8_RETURN_2 ( find_first_of, str, cset . str -> s );

        if ( xright > cnt )
            xright = cnt;

        // walk across string, one character at a time
        long int save_idx = idx;
        long int save_off = off;
        Iterator & a = * this;
        for ( count_t i = left; a . isValid () && i < xright; ++ a, ++ i )
        {
            // retrieve current character
            UTF32 ch_a = * a;

            // walk across set
            Iterator b = cset . makeIterator ();
            for ( ; b . isValid (); ++ b )
            {
                // return a match at this character index
                if ( ch_a == * b )
                    return true;
            }
        }

        idx = save_idx;
        off = save_off;

        return false;
    }

    bool String :: Iterator :: findFirstOf ( const UTF8 * zcset, count_t len )
    {
        DETECT_NULL_ZSTR ( zcset );
        STRING_IT_FIND_DECLARE_VARS_3 ( findFirstOf, zcset, count_t left, xright );

        if ( zcset [ 0 ] == 0 )
        {
            if ( idx < 0 )
            {
                idx = 0;
                off = 0;
            }
            assert ( ( count_t ) idx < cnt );
            return true;
        }

        STRING_IT_FIND_ASCII_RETURN_1 ( find_first_of, str, zcset, nsquare_search_edge );
    }

    bool String :: Iterator :: findFirstOf ( const UTF8 * zcset, const Iterator & xend )
    {
        DETECT_NULL_ZSTR ( zcset );
        STRING_IT_FIND_DECLARE_VARS_4 ( findFirstOf, zcset, count_t left, xright );

        if ( zcset [ 0 ] == 0 )
        {
            if ( idx < 0 )
            {
                idx = 0;
                off = 0;
            }
            assert ( ( count_t ) idx < cnt );
            return true;
        }

        STRING_IT_FIND_ASCII_RETURN_1 ( find_first_of, str, zcset, nsquare_search_edge );
    }

    bool String :: Iterator :: findLastOf ( const String & cset, count_t len )
    {
        STRING_IT_RFIND_DECLARE_VARS_1 ( count_t left, xright );

        if ( cset . isEmpty () )
        {
            if ( ( count_t ) idx >= cnt )
            {
                // seek right edge
                idx = cnt;
                off = str -> s . size ();

                // back up by one
                -- ( * this );
            }
            assert ( idx >= 0 );
            return true;
        }

        if ( isAscii () )
            STRING_IT_RFIND_ASCII_RETURN_1 ( find_last_of, str, cset . str -> s, nsquare_search_edge );

        if ( cset . isAscii () )
            STRING_IT_RFIND_UTF8_RETURN_1 ( find_last_of, str, cset . str -> s );

        // walk across string, one character at a time
        long int save_idx = idx;
        long int save_off = off;
        Iterator & a = * this;
        for ( count_t i = xright; a . isValid () && i > left; -- a, -- i )
        {
            // retrieve current character
            UTF32 ch_a = * a;

            // walk across set
            Iterator b = cset . makeIterator ();
            for ( ; b . isValid (); ++ b )
            {
                // return a match at this character index
                if ( ch_a == * b )
                    return true;
            }
        }

        idx = save_idx;
        off = save_off;

        return false;
    }

    bool String :: Iterator :: findLastOf ( const String & cset, const Iterator & end )
    {
        STRING_IT_RFIND_DECLARE_VARS_2 ( count_t left, xright );

        if ( cset . isEmpty () )
        {
            if ( ( count_t ) idx >= cnt )
            {
                idx = cnt;
                off = str -> s . size ();
                -- ( * this );
            }
            assert ( idx >= 0 );
            return true;
        }

        if ( isAscii () )
            STRING_IT_RFIND_ASCII_RETURN_1 ( find_last_of, str, cset . str -> s, nsquare_search_edge );

        if ( cset . isAscii () )
            STRING_IT_RFIND_UTF8_RETURN_2 ( find_last_of, str, cset . str -> s );

        // walk across string, one character at a time
        long int save_idx = idx;
        long int save_off = off;
        Iterator & a = * this;
        for ( count_t i = xright; a . isValid () && i > left; -- a, -- i )
        {
            // retrieve current character
            UTF32 ch_a = * a;

            // walk across set
            Iterator b = cset . makeIterator ();
            for ( ; b . isValid (); ++ b )
            {
                // return a match at this character index
                if ( ch_a == * b )
                    return true;
            }
        }

        idx = save_idx;
        off = save_off;

        return false;
    }

    bool String :: Iterator :: findLastOf ( const UTF8 * zcset, count_t len )
    {
        DETECT_NULL_ZSTR ( zcset );
        STRING_IT_RFIND_DECLARE_VARS_3 ( findLastOf, zcset, count_t left, xright );
        STRING_IT_RFIND_ASCII_RETURN_1 ( find_last_of, str, zcset, nsquare_search_edge );
    }

    bool String :: Iterator :: findLastOf ( const UTF8 * zcset, const Iterator & end )
    {
        DETECT_NULL_ZSTR ( zcset );
        STRING_IT_RFIND_DECLARE_VARS_4 ( findLastOf, zcset, count_t left, xright );
        STRING_IT_RFIND_ASCII_RETURN_1 ( find_last_of, str, zcset, nsquare_search_edge );
    }

    String :: Iterator & String :: Iterator :: operator ++ ()
    {
        if ( str == nullptr )
            throw IteratorInvalid ( XP ( XLOC ) << "source String has been destroyed" );

        if ( isAscii () )
            ++ off;
        else
            off += scan_fwd ( str -> s . data (), str -> s . size (), off );

        ++ idx;

        return * this;
    }

    String :: Iterator & String :: Iterator :: operator -- ()
    {
        if ( str == nullptr )
            throw IteratorInvalid ( XP ( XLOC ) << "source String has been destroyed" );

        if ( isAscii () )
            -- off;
        else
            off -= scan_rev ( str -> s . data (), str -> s . size (), off );

        -- idx;

        return * this;
    }

    bool String :: Iterator :: operator == ( const Iterator & it ) const noexcept
    {
        // neither guy can be null
        if ( str == nullptr )
            return false;
        if ( it . str == nullptr )
            return false;

        // if they refer to the same string, not just same value
        if ( str != it . str )
            return false;
        if ( cnt != it . cnt )
            return false;

        // they have to point at the same location
        if ( off != it . off )
            return false;

        return true;
    }

    bool String :: Iterator :: operator != ( const Iterator & it ) const noexcept
    {
        return ! operator == ( it );
    }

    long int String :: Iterator :: operator - ( const Iterator & it ) const
    {
        if ( str == nullptr || it . str == nullptr )
            throw InvalidIterator ( XP ( XLOC ) << "one or both iterstors are orphaned" );
        if ( str != it . str )
            throw IteratorsDiffer ( XP ( XLOC ) << "iterators refer to different Strings" );

        return idx - it . idx;
    }

    String :: Iterator & String :: Iterator :: operator += ( long int adjust )
    {
        if ( adjust < 0 )
            return operator -= ( - adjust );

        if ( str == nullptr )
            throw IteratorInvalid ( XP ( XLOC ) << "source String has been destroyed" );

        if ( isAscii () )
            off += adjust;
        else
        {
            // characters before end of string
            count_t avail = cnt - idx;

            // amount to measure is shorter of "avail" or "adjust"
            count_t amt = ( avail > ( count_t ) adjust ) ? ( count_t ) adjust : avail;

            // the number of extra characters off end of string
            count_t extra = avail - amt;
            if ( ( count_t ) adjust >= avail )
            {
                // add in any overage
                off = str -> s . size () + extra;
            }
            else
            {
                // measure the size
                off += char_idx_to_byte_off (
                    str -> s . data () + off,
                    str -> s . size () - off,
                    amt );
            }
        }

        idx += adjust;

        return * this;
    }

    String :: Iterator & String :: Iterator :: operator -= ( long int adjust )
    {
        if ( adjust < 0 )
            return operator += ( - adjust );

        if ( str == nullptr )
            throw IteratorInvalid ( XP ( XLOC ) << "source String has been destroyed" );

        if ( isAscii () )
            off -= adjust;
        else
        {
            // characters before beginngin of string
            count_t avail = idx;

            // amount to measure is shorter of "avail" or "adjust"
            count_t amt = ( avail > ( count_t ) adjust ) ? ( count_t ) adjust : avail;

            // the number of extra characters off end of string
            count_t extra = avail - amt;
            if ( ( count_t ) adjust >= avail )
            {
                off = 0L - extra;
            }
            else
            {
                // the new left-edge
                count_t left = idx - amt;
                if ( left < amt )
                {
                    // measure the size to left
                    off = char_idx_to_byte_off (
                        str -> s . data (),
                        str -> s . size (),
                        left );
                }
                else
                {
                    for ( count_t i = 0; i < amt; ++ i )
                    {
                        off -= scan_rev ( str -> s . data (), str -> s . size (), off );
                    }
                }
            }
        }

        idx -= adjust;

        return * this;
    }

    String :: Iterator & String :: Iterator :: operator = ( const Iterator & it )
    {
        str = it . str;
        cnt = it . cnt;
        idx = it . idx;
        off = it . off;
        return * this;
    }

    String :: Iterator :: Iterator ( const Iterator & it )
        : str ( it . str )
        , cnt ( it . cnt )
        , idx ( it . idx )
        , off ( it . off )
    {
    }

    String :: Iterator & String :: Iterator :: operator = ( const Iterator && it )
    {
        str = std :: move ( it . str );
        cnt = it . cnt;
        idx = it . idx;
        off = it . off;
        return * this;
    }

    String :: Iterator :: Iterator ( const Iterator && it )
        : str ( std :: move ( it . str ) )
        , cnt ( it . cnt )
        , idx ( it . idx )
        , off ( it . off )
    {
    }

    String :: Iterator :: Iterator ()
        : cnt ( 0 )
        , idx ( 0 )
        , off ( 0 )
    {
    }

    String :: Iterator :: ~ Iterator ()
    {
    }

    String :: Iterator :: Iterator ( const SRef < Wiper > & _str, count_t _cnt, size_t origin, count_t index )
        : str ( _str )
        , cnt ( _cnt )
        , idx ( index )
        , off ( origin )
    {
    }


    /*=================================================*
     *               NULTerminatedString               *
     *=================================================*/

    const UTF8 * NULTerminatedString :: c_str () const noexcept
    {
        return str -> s . c_str ();
    }

    NULTerminatedString & NULTerminatedString :: operator = ( const String & s )
    {
        String :: operator = ( s );
        return * this;
    }

    NULTerminatedString :: NULTerminatedString ( const String & s )
        : String ( s )
    {
    }

    NULTerminatedString & NULTerminatedString :: operator = ( const NULTerminatedString & zs )
    {
        String :: operator = ( zs );
        return * this;
    }

    NULTerminatedString :: NULTerminatedString ( const NULTerminatedString & zs )
        : String ( zs )
    {
    }

    NULTerminatedString & NULTerminatedString :: operator = ( const NULTerminatedString && zs )
    {
        String :: operator = ( std :: move ( zs ) );
        return * this;
    }

    NULTerminatedString :: NULTerminatedString ( const NULTerminatedString && zs )
        : String ( std :: move ( zs ) )
    {
    }


    /*=================================================*
     *                  StringBuffer                   *
     *=================================================*/


    bool StringBuffer :: isEmpty () const
    {
        SLocker lock ( busy );
        return str . isEmpty ();
    }

    bool StringBuffer :: isAscii () const
    {
        SLocker lock ( busy );
        return str . isAscii ();
    }

    size_t StringBuffer :: size () const
    {
        SLocker lock ( busy );
        return str . size ();
    }

    size_t StringBuffer :: capacity () const
    {
        SLocker lock ( busy );
        return str . str -> s . capacity ();
    }

    count_t StringBuffer :: count () const
    {
        SLocker lock ( busy );
        return str . count ();
    }

    count_t StringBuffer :: length () const
    {
        SLocker lock ( busy );
        return str . length ();
    }

    const UTF8 * StringBuffer :: data () const
    {
        SLocker lock ( busy );
        return str . data ();
    }

    UTF32 StringBuffer :: getChar ( count_t idx ) const
    {
        SLocker lock ( busy );
        return str . getChar ( idx );
    }

    bool StringBuffer :: equal ( const String & s ) const
    {
        SLocker lock ( busy );
        return str . equal ( s );
    }

    bool StringBuffer :: equal ( const StringBuffer & sb ) const
    {
        SLocker lock1 ( busy );
        SLocker lock2 ( sb . busy );
        return str . equal  ( sb . str );
    }

    int StringBuffer :: compare ( const String & s ) const
    {
        SLocker lock ( busy );
        return str . compare ( s );
    }

    int StringBuffer :: compare ( const StringBuffer & sb ) const
    {
        SLocker lock1 ( busy );
        SLocker lock2 ( sb . busy );
        return str . compare ( sb . str );
    }

    int StringBuffer :: caseInsensitiveCompare ( const String & s ) const
    {
        SLocker lock ( busy );
        return str . caseInsensitiveCompare ( s );
    }

    int StringBuffer :: caseInsensitiveCompare ( const StringBuffer & sb ) const
    {
        SLocker lock1 ( busy );
        SLocker lock2 ( sb . busy );
        return str . caseInsensitiveCompare ( sb . str );
    }

    count_t StringBuffer :: find ( const String & sub, count_t left, count_t len ) const
    {
        SLocker lock ( busy );
        return str . find ( sub, left, len );
    }

    count_t StringBuffer :: find ( const UTF8 * zstr, count_t left, count_t len ) const
    {
        SLocker lock ( busy );
        return str . find ( zstr, left, len );
    }

    count_t StringBuffer :: find ( UTF32 ch, count_t left, count_t len ) const
    {
        SLocker lock ( busy );
        return str . find ( ch, left, len );
    }

    count_t StringBuffer :: rfind ( const String & sub, count_t xright, count_t len ) const
    {
        SLocker lock ( busy );
        return str . rfind ( sub, xright, len );
    }

    count_t StringBuffer :: rfind ( const UTF8 * zstr, count_t xright, count_t len ) const
    {
        SLocker lock ( busy );
        return str . rfind ( zstr, xright, len );
    }

    count_t StringBuffer :: rfind ( UTF32 ch, count_t xright, count_t len ) const
    {
        SLocker lock ( busy );
        return str . rfind ( ch, xright, len );
    }

    count_t StringBuffer :: findFirstOf ( const String & cset, count_t left, count_t len ) const
    {
        SLocker lock ( busy );
        return str . findFirstOf ( cset, left, len );
    }

    count_t StringBuffer :: findFirstOf ( const UTF8 * zcset, count_t left, count_t len ) const
    {
        SLocker lock ( busy );
        return str . findFirstOf ( zcset, left, len );
    }

    count_t StringBuffer :: findLastOf ( const String & cset, count_t xright, count_t len ) const
    {
        SLocker lock ( busy );
        return str . findLastOf ( cset, xright, len );
    }

    count_t StringBuffer :: findLastOf ( const UTF8 * zcset, count_t xright, count_t len ) const
    {
        SLocker lock ( busy );
        return str . findLastOf ( zcset, xright, len );
    }

    bool StringBuffer :: beginsWith ( const String & sub ) const
    {
        SLocker lock ( busy );
        return str . beginsWith ( sub );
    }

    bool StringBuffer :: beginsWith ( const UTF8 * zsub ) const
    {
        SLocker lock ( busy );
        return str . beginsWith ( zsub );
    }

    bool StringBuffer :: beginsWith ( UTF32 ch ) const
    {
        SLocker lock ( busy );
        return str . beginsWith ( ch );
    }

    bool StringBuffer :: endsWith ( const String & sub ) const
    {
        SLocker lock ( busy );
        return str . endsWith ( sub );
    }

    bool StringBuffer :: endsWith ( const UTF8 * zsub ) const
    {
        SLocker lock ( busy );
        return str . endsWith ( zsub );
    }

    bool StringBuffer :: endsWith ( UTF32 ch ) const
    {
        SLocker lock ( busy );
        return str . endsWith ( ch );
    }
    
    StringBuffer & StringBuffer :: append ( const String & s )
    {
        XLocker lock ( busy );
        str . str -> append ( * s . str );
        str . cnt += s . cnt;
        return * this;
    }

    StringBuffer & StringBuffer :: append ( const UTF8 * zstr )
    {
        if ( zstr == nullptr )
            throw NullArgumentException ( XP ( XLOC ) << "null pointer when expecting a NUL-terminated string" );

        if ( is_ascii ( zstr ) )
        {
            XLocker lock ( busy );
            size_t old_size = str . size ();
            str . str -> s . append ( zstr );
            size_t new_size = str . size ();
            str . cnt += new_size - old_size;
            return * this;
        }

        return append ( String ( zstr ) );
    }

    StringBuffer & StringBuffer :: append ( UTF32 ch )
    {
        if ( is_ascii ( ch ) )
        {
            XLocker lock ( busy );
            str . str -> s += ( char ) ch;
            str . cnt += 1;
            return * this;
        }

        return append ( String ( ch ) );
    }

    StringBuffer & StringBuffer :: toupper ()
    {
        XLocker lock ( busy );

        if ( str . isAscii () )
        {
            ASCII * p = ( ASCII * ) str . data ();
            size_t sz = str . size ();
            for ( size_t i = 0; i < sz; ++ i )
            {
                if ( :: islower ( p [ i ] ) )
                    p [ i ] = :: toupper ( p [ i ] );
            }
        }
        else
        {
            auto it = str . makeIterator ();
            for ( ; it . isValid (); ++ it )
            {
                UTF32 ch = it . get ();
                if ( iswlower ( ch ) )
                {
                    size_t cur_sz = scan_fwd ( str . data (), str . size (), it . byteOffset () );

                    ch = :: towupper ( ch );

                    UTF8 buffer [ 8 ];
                    size_t new_sz = utf32_to_utf8_strict ( buffer, sizeof buffer, ch );

                    str . str -> s . replace ( it . byteOffset (), cur_sz, buffer, new_sz );
                }
            }
        }
        return * this;
    }

    StringBuffer & StringBuffer :: tolower ()
    {
        XLocker lock ( busy );

        if ( str . isAscii () )
        {
            ASCII * p = ( ASCII * ) str . data ();
            size_t sz = str . size ();
            for ( size_t i = 0; i < sz; ++ i )
            {
                if ( :: isupper ( p [ i ] ) )
                    p [ i ] = :: tolower ( p [ i ] );
            }
        }
        else
        {
            auto it = str . makeIterator ();
            for ( ; it . isValid (); ++ it )
            {
                UTF32 ch = it . get ();
                if ( iswupper ( ch ) )
                {
                    size_t cur_sz = scan_fwd ( str . data (), str . size (), it . byteOffset () );

                    ch = :: towlower ( ch );

                    UTF8 buffer [ 8 ];
                    size_t new_sz = utf32_to_utf8_strict ( buffer, sizeof buffer, ch );

                    str . str -> s . replace ( it . byteOffset (), cur_sz, buffer, new_sz );
                }
            }
        }
        return * this;
    }

    void StringBuffer :: clear ( bool wipe )
    {
        XLocker lock ( busy );
        str . clear ( wipe );
    }

    String StringBuffer :: toString () const
    {
        SLocker lock ( busy );
        return String ( str );
    }

    String StringBuffer :: stealString ()
    {
        XLocker lock ( busy );
        return String ( std :: move ( str ) );
    }

    StringBuffer & StringBuffer :: operator = ( const StringBuffer & sb )
    {
        XLocker lock1 ( busy );
        SLocker lock2 ( sb . busy );
        str = sb . str;
        return * this;
    }

    StringBuffer :: StringBuffer ( const StringBuffer & sb )
    {
        SLocker lock ( sb . busy );
        str = sb . str;
    }

    StringBuffer & StringBuffer :: operator = ( const StringBuffer && sb )
    {
        XLocker lock1 ( busy );
        SLocker lock2 ( sb . busy );
        str = std :: move ( sb . str );
        return * this;
    }

    StringBuffer :: StringBuffer ( const StringBuffer && sb )
    {
        XLocker lock ( sb . busy );
        str = sb . str;
    }

    StringBuffer :: StringBuffer ()
    {
    }

    StringBuffer :: ~ StringBuffer ()
    {
    }

#undef DETECT_NULL_ZSTR
#undef DETECT_MISMATCHED_STRINGS
#undef STRING_FIND_DECLARE_VARS_1
#undef STRING_FIND_DECLARE_VARS_2
#undef STRING_FIND_DECLARE_VARS_3
#undef STRING_FIND_ASCII_RETURN_1
#undef STRING_FIND_UTF8_RETURN_1
#undef STRING_RFIND_DECLARE_VARS_1
#undef STRING_RFIND_DECLARE_VARS_2
#undef STRING_RFIND_DECLARE_VARS_3
#undef STRING_RFIND_ASCII_RETURN_1
#undef STRING_RFIND_UTF8_RETURN_1
#undef STRING_IT_FIND_CALC_VARS_1
#undef STRING_IT_FIND_DECLARE_VARS_1
#undef STRING_IT_FIND_CALC_VARS_2
#undef STRING_IT_FIND_DECLARE_VARS_2
#undef STRING_IT_FIND_DECLARE_VARS_3
#undef STRING_IT_FIND_DECLARE_VARS_4
#undef STRING_IT_FIND_ASCII_RETURN_1
#undef STRING_IT_FIND_UTF8_RETURN_1
#undef STRING_IT_FIND_UTF8_RETURN_2
#undef STRING_IT_RFIND_CALC_VARS_1
#undef STRING_IT_RFIND_DECLARE_VARS_1
#undef STRING_IT_RFIND_CALC_VARS_2
#undef STRING_IT_RFIND_DECLARE_VARS_2
#undef STRING_IT_RFIND_DECLARE_VARS_3
#undef STRING_IT_RFIND_DECLARE_VARS_4
#undef STRING_IT_RFIND_ASCII_RETURN_1
#undef STRING_IT_RFIND_UTF8_RETURN_1
#undef STRING_IT_RFIND_UTF8_RETURN_2

}
