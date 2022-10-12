/* ===========================================================================
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
* Project:
*  sratools command line tool
*
* Purpose:
*  Parse SDL Response JSON
*
*/

#pragma once

#include <utility>
#include <cassert>

#if MS_Visual_C
#pragma warning(disable: 4242)
#pragma warning(disable: 4244)
#endif


/// This is a basic unchecked unicode character, i.e. this code will let you generate invalid UTF-8.
///
/// It will generate UTF-32 for codepoints beyond the Unicode maximum (0x10FFFF), up to UINT32_MAX.
/// It will generate UTF-8 for codepoints beyond the Unicode maximum (0x10FFFF), up to 0x1FFFFF.
/// It will generate UTF-8 for codepoints in the UTF-16 surrogate range [ 0x0D800 .. 0x0DFFF ].
/// The proper handling of UTF-16 surrogate pairs is up to the caller.
/// But this class has some functions to help with that.
/// \code
/// if (cur.is_surrogate_first() && next.is_surrogate_second()) {
///     cur.combine(next);
/// }
/// \endcode
/// And make sure you don't try to convert any incomplete UTF-16 code units.
/// \code
/// if (!cur.is_surrogate()) {
///     uint8_t buffer[4];
///     std::string s((char *)buffer, cur.utf8(buffer));
///     std::cout << s;
/// }
/// else {
///     throw std::range_error("incomplete UTF-16 surrogate pair");
/// }
/// \endcode
struct UnicharBasic {
    uint32_t codepoint;

    bool operator== (UnicharBasic const &lhs) const { return codepoint == lhs.codepoint; }
    bool operator!= (UnicharBasic const &lhs) const { return codepoint != lhs.codepoint; }
    bool operator< (UnicharBasic const &lhs) const { return codepoint < lhs.codepoint; }
    bool operator<= (UnicharBasic const &lhs) const { return codepoint <= lhs.codepoint; }
    bool operator> (UnicharBasic const &lhs) const { return codepoint > lhs.codepoint; }
    bool operator>= (UnicharBasic const &lhs) const { return codepoint >= lhs.codepoint; }

    UnicharBasic(uint32_t codepoint_) : codepoint(codepoint_) {}
    UnicharBasic(UnicharBasic const &) = default;
    UnicharBasic(UnicharBasic &&) = default;
    UnicharBasic &operator= (UnicharBasic const &) = default;
    UnicharBasic &operator= (UnicharBasic &&) = default;
    ~UnicharBasic() = default;

    /// @brief The current Unicode standard sets the maximum codepoint to be the biggest that can be encoded in UTF-16.
    static uint32_t constexpr max = (1UL << 20) + 0x10000 - 1;
    static uint32_t constexpr surrogate_first = 0xD800;
    static uint32_t constexpr surrogate_second = 0xDC00;
    static uint32_t constexpr surrogate_end = 0xE000;

    
    
    /// @brief Decode/Accumulate UTF-8 code units into a Unichar.
    ///
    /// @param ch the UTF-8 code unit to decode/accumulate.
    /// @param n the counter, use 0 for first byte, otherwise use the last returned counter > 1.
    /// @param accum the accumulated value.
    ///
    /// @returns a counter; on error, the counter is 0; when done, it is 1.
    unsigned decode8(uint8_t chu, unsigned n = 0) {
        uint32_t const ch = chu;
        if (n == 0) {
            // The 1 byte encoding has 1 encoding bit  and 7 value bits, like 0xxxxxxx.
            // The 2 byte encoding has 3 encoding bits and 5 value bits, like 110xxxxx.
            // The 3 byte encoding has 4 encoding bits and 4 value bits, like 1110xxxx.
            // The 4 byte encoding has 5 encoding bits and 3 value bits, like 11110xxx.
            if (ch <= 0x7F) { ///< the 1 byte encoding
                codepoint = ch;
                return 1;
            }

            uint8_t MV = (1 << 5) - 1; ///< mask for the value
            for (unsigned i = 2; i <= 4; ++i, MV >>= 1) {
                uint8_t const ME = ~MV; // mask for the encoding bits, e.g. 0xE0
                uint8_t const EV = ME << 1; // value of the encoding bits, e.g. 0xC0
                
                if ((ch & ME) == EV && (uint8_t)ch <= EV + MV) {
                    codepoint = ch & MV;
                    return i;
                }
            }
        }
        else if (n <= 4 && (ch & 0xC0) == 0x80) {
            // The remaining bytes have 2 encoding bits and 6 value bits, like 10xxxxxx.
            codepoint = (codepoint << 6) | (ch & 0x3F);
            return n - 1;
        }
        return 0;
    }
    
    bool is_surrogate_first() const {
        return surrogate_first <= codepoint && codepoint < surrogate_second;
    }
    bool is_surrogate_second() const {
        return surrogate_second <= codepoint && codepoint < surrogate_end;
    }
    bool is_surrogate() const {
        return surrogate_first <= codepoint && codepoint < surrogate_end;
    }
    UnicharBasic &combine(UnicharBasic const &second) {
        assert(is_surrogate_first() && second.is_surrogate_second());
        codepoint = 0x10000 + ((codepoint - surrogate_first) << 10) + (second.codepoint - surrogate_second);
        return *this;
    }
    static UnicharBasic combine(UnicharBasic const &first, UnicharBasic const &second) {
        return UnicharBasic(first).combine(second);
    }
    
    operator bool() const { return !!codepoint; }
    bool is_valid_unicode() const { return codepoint <= max; }
    bool is_valid_as_UTF8() const { return is_valid_unicode() && !is_surrogate(); }

    /// @brief Encode the codepoint as UTF-8 code units.
    /// @returns the number of code units produced, or -1 on error.
    int utf8(uint8_t out[4]) const {
        out[0] = 0;
        if (codepoint == 0)
            return 0;
        if (codepoint < (1ul << 7)) {
            // 0xxxxxxx
            out[0] = codepoint;
            return 1;
        }
        if ((codepoint >> 6) < (1ul << 5)) {
            // 110xxxxx 10xxxxxx
            // 11000011 10011000
            out[0] = ((codepoint >> 6) & 0x1F) | 0xC0;
            out[1] = ((codepoint >> 0) & 0x3F) | 0x80;
            return 2;
        }
        if ((codepoint >> 12) < (1ul << 4)) {
            // 1110xxxx 10xxxxxx 10xxxxxx
            out[0] = ((codepoint >> 12) & 0x0F) | 0xE0;
            out[1] = ((codepoint >>  6) & 0x3F) | 0x80;
            out[2] = ((codepoint >>  0) & 0x3F) | 0x80;
            return 3;
        }
        if ((codepoint >> 18) < (1ul << 3)) {
            // 11110100 10001111 10111111 10111111
            // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            out[0] = ((codepoint >> 18) & 0x07) | 0xF0;
            out[1] = ((codepoint >> 12) & 0x3F) | 0x80;
            out[2] = ((codepoint >>  6) & 0x3F) | 0x80;
            out[3] = ((codepoint >>  0) & 0x3F) | 0x80;
            return 4;
        }
        return -1;
    }
    int utf16(uint16_t out[2]) const {
        out[0] = 0;
        if (codepoint == 0)
            return 0;
        if (codepoint < 0x10000) {
            out[0] = codepoint;
            return 1;
        }
        if (codepoint <= max) {
            auto const a = codepoint - 0x10000;
            auto const s = (a & 0x3FF) + surrogate_second;
            auto const f = (a  >>  10) + surrogate_first;
            
            assert(combine(f, s).codepoint == codepoint);
            out[0] = f;
            out[1] = s;
            return 2;
        }
        return -1;
    }
    int utf32(uint32_t out[1]) const {
        out[0] = codepoint;
        return codepoint == 0 ? 0 : 1;
    }
};
