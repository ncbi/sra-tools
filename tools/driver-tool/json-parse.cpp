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

#include <string>
#include <vector>

#include "opt_string.hpp"
#include "json-parse.hpp"
#include "utf8-16.hpp"

static inline bool isword(int const ch) {
    switch (ch) {
    case ',':
    case ']':
    case '}':
        return false;
    }
    return !isspace(ch);
}

static inline bool isvalue(int const ch) {
    switch (ch) {
    case '-': // sign
    case 't': // true
    case 'f': // false
    case 'n': // null
        return true;
    default:
    return ch >= '0' && ch <= '9';
    }
}

inline void JSONParser::word() {
    try {
        for (auto ch = next(); isword(ch); ch = next())
            ;
        --cur;
    }
    catch (EndOfInput const &e) {
        (void)(e);
        return;
    }
}

inline void JSONParser::string() {
    auto escape = false;
    for (auto ch = next(); ch > 0; ch = next()) {
        if (!escape) {
            if (ch == '"') {
                return;
            }
            if (ch == '\\')
                escape = true;
            continue;
        }
        escape = false;
    }
}

 void JSONParser::value() {
START_VALUE:
    if (expect('[')) {
        {
            auto const at = cur - 1;
            DEBUG_OUT << "array start " << (void *)at << std::endl;
            send(evt_array, at, at);
        }
        push_state(false);
        if (expect(']'))
            goto END_ARRAY;
        goto START_VALUE;
    }
    if (expect('{')) {
        {
            auto const at = cur - 1;
            DEBUG_OUT << "object " << (void *)(at) << std::endl;
            send(evt_object, at, at);
        }
        push_state(true);
        if (expect('}'))
            goto END_OBJECT;
START_MEMBER: // comes from expect(',') in END_VALUE
        if (expect('"')) {
            auto const start = cur;
            string();
            DEBUG_OUT << "member " << std::string(start, cur - 1) << std::endl;
            send(evt_member_name, start, cur - 1);
        }
        else {
            DEBUG_OUT << "expected a member name" << std::endl;
            throw ExpectedMemberName();
        }
        if (!expect(':')) {
            DEBUG_OUT << "expected a ':' after member name" << std::endl;
            throw ExpectedMemberNameValueSeparator();
        }
        goto START_VALUE; ///< parse member value
    }
    if (expect('"')) {
        auto const start = cur - 1;
        string();
        DEBUG_OUT << "string " << std::string(start, cur) << std::endl;
        send(evt_scalar, start, cur);
        goto END_VALUE;
    }
    else if (isvalue(*cur)) {
        auto const start = cur;
        word();
        if (start == cur) {
            DEBUG_OUT << "no value" << std::endl;
            throw ExpectedSomeValue();
        }
        DEBUG_OUT << "scalar " << std::string(start, cur) << std::endl;
        send(evt_scalar, start, cur);
        goto END_VALUE;
    }
    goto UNEXPECTED;

END_VALUE:
    if (expect(',')) {
        if (is_parsing_object())
            goto START_MEMBER; ///< parse next member
        if (is_parsing_array())
            goto START_VALUE; ///< parse next item
        DEBUG_OUT << "',' not allowed here" << std::endl;
        throw Unexpected(',');
    }
    else if (expect(']')) {
END_ARRAY:
        if (is_parsing_array()) {
            auto const at = cur - 1;
            DEBUG_OUT << "array end " << (void *)at << std::endl;
            send(evt_end, at, at);
            if (pop_state())
                goto END_VALUE;
            return; ///< done
        }
    }
    else if (expect('}')) {
END_OBJECT:
        if (is_parsing_object()) {
            auto const at = cur - 1;
            DEBUG_OUT << "object end " << (void *)at << std::endl;
            send(evt_end, at, at);
            if (pop_state())
                goto END_VALUE;
            return; ///< done
        }
    }
UNEXPECTED:
    DEBUG_OUT << "unexpected '" << *cur << "'" << std::endl;
    throw Unexpected(*cur);
}

struct UnicharTests;

struct Unichar : protected UnicharBasic {
    explicit Unichar(uint32_t codepoint = 0) : UnicharBasic(codepoint) {}
    Unichar(Unichar const &) = default;
    Unichar(Unichar &&) = default;
    Unichar &operator= (Unichar const &) = default;
    Unichar &operator= (Unichar &&) = default;

protected:
    uint8_t complete = 0;
    Unichar(UnicharBasic const &u) : UnicharBasic(u) {};
    Unichar(UnicharBasic &&u) : UnicharBasic(std::move(u)) {};

public:
    using UnicharBasic::is_valid_as_UTF8;

    void reset() {
        codepoint = 0;
        complete = 0;
    }
    /// @Returns true if there are no more code units needed to complete the value.
    /// @Throws JSONStringConversionInvalidUTF8Error
    bool add_UTF8(int codeunit) {
        complete = UnicharBasic::decode8((uint8_t)codeunit, complete);
        if (complete > 0)
            return complete == 1;
        throw JSONStringConversionInvalidUTF8Error();
    }
    /// @Returns true if there are no more code units needed to complete the value.
    /// @Throws JSONStringConversionInvalidUTF16Error
    bool add_UTF16(int codeunit) {
        if (complete <= 1) {
            codepoint = (uint16_t)codeunit;
            if (!is_surrogate()) {
                complete = 1;
                return true;
            }
            if (is_surrogate_first()) {
                complete = 2;
                return false;
            }
        }
        else if (complete == 2) {
            auto const second = UnicharBasic{(uint16_t)codeunit};
            if (second.is_surrogate_second()) {
                combine(second);
                complete = 1;
                return true;
            }
        }
        throw JSONStringConversionInvalidUTF16Error();
    }
    /// @Returns true if there are no more characters needed to complete the value.
    /// @Throws JSONStringConversionInvalidUTF16Error
    bool add_Hex16(char x) {
        int val = 0;
        if (isdigit(x))
            val = x - '0';
        else if (isxdigit(x))
            val = (toupper(x) - 'A') + 10;
        else
            throw JSONStringConversionInvalidUTF16Error();
        if (complete <= 1) {
            codepoint = val;
            complete = 4;
            return false;
        }
        if (complete <= 5) {
            // we might have stashed the first surrogate in the high 16 bits, make sure not to damage it.
            UnicharBasic const hi{codepoint >> 16};
            UnicharBasic lo{codepoint & 0xFFFF};

            lo.codepoint = (lo.codepoint << 4) | val;
            assert(lo.codepoint < 0x10000);

            --complete;
            if (complete > 1) {
                codepoint = (hi.codepoint << 16) | lo.codepoint;
                return false;
            }
            if (lo.is_surrogate_first()) {
                // stash it in the high 16 bits
                codepoint = lo.codepoint << 16;
                complete = 5;
                return false;
            }
            if (hi.is_surrogate_first() && lo.is_surrogate_second()) {
                codepoint = combine(hi, lo).codepoint;
                return true;
            }
            if (hi.codepoint == 0) {
                codepoint = lo.codepoint;
                return true;
            }
        }
        throw JSONStringConversionInvalidUTF16Error();
    }
    int utf8(char out[4]) const {
        if (0 <= complete && complete <= 1 && is_valid_as_UTF8()) {
            auto const rslt = UnicharBasic::utf8(reinterpret_cast<uint8_t *>(out));
            if (rslt >= 0)
                return rslt;
        }
        throw JSONStringConversionInvalidUTF8Error();
    }
    void append(std::string &target) const {
        char buffer[4];
        target.append(buffer, utf8(buffer));
    }
#if DEBUG || _DEBUGGING
    static void runTests();
#endif
};


#if PEDANTIC_MEMBER_NAMES
struct ValidMemberCharacters {
private:
    uint8_t valid[128];

public:
    bool operator ()(int const ch, bool const isFirst) const {
        if (!(0 < ch && ch < 128))
            return false;
        return (valid[ch] & 127) == ch && (!isFirst || ((valid[ch] & 128) != 0));
    }
    ValidMemberCharacters() {
        for (auto i = 0; i < 128; ++i) {
            // [A-Za-z_][A-Za-z0-9_]*

            int v = 0;

            if (i >= '0' && i <= '9')
                v = i;
            else if (i >= 'A' && i <= 'Z')
                v = i | 128;
            else if (i >= 'a' && i <= 'z')
                v = i | 128;
            else if (i == '_')
                v = i | 128;

            valid[i] = v;
        }
    }
};
static auto const validMemberCharacters = ValidMemberCharacters();
#endif

// decode JSON string to UTF8 std::string
JSONString::operator std::string() const {
    std::string result;
    int hex = 0;
    bool escape = false;
    bool utf = false;
    Unichar chu; // this is only used for validating

    for (auto chi : value) {
        assert(chi != 0);

        if (!escape && chi == '\\') {
            if (hex == 0 && !utf) {
                escape = true;
                continue;
            }
            throw JSONScalarConversionError();
        }
        if (escape) {
            escape = false;
            switch (chi) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                chi = '\b';
                break;
            case 'f':
                chi = '\f';
                break;
            case 'n':
                chi = '\n';
                break;
            case 'r':
                chi = '\r';
                break;
            case 't':
                chi = '\t';
                break;
            case 'u':
                hex = 4;
                continue;
            default:
                throw JSONScalarConversionError();
            }
            result.append(1, chi);
        }
        else if (hex > 0) {
            assert(!utf);
            --hex;
            if (chu.add_Hex16(chi))
                chu.append(result);
        }
        else {
            result.append(1, chi);
            if (utf) {
                if (chu.add_UTF8(chi)) {
                    if (!chu.is_valid_as_UTF8())
                        throw JSONScalarConversionError();
                    utf = false;
                }
            }
            else if (chu.add_UTF8(chi))
                chu.reset();
            else
                utf = true;
        }
    }
    if (hex > 0)
        throw JSONScalarConversionError();
    if (utf)
        throw JSONScalarConversionError();
    if (escape)
        throw JSONScalarConversionError();
#if PEDANTIC_MEMBER_NAMES
    if (isMemberName) {
        bool first = true;
        for (auto const &ch : result) {
            if (!validMemberCharacters(ch, first))
                throw BadMemberNameError();
            first = false;
        }
    }
#endif
    return result;
#undef VALIDATE
}
