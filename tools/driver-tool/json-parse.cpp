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
#if MS_Visual_C
        return ch >= '0' && ch <= '9';
#else
        return isnumber(ch);
#endif
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

#if (DEBUG || _DEBUGGING) && !WINDOWS
// MARK: Tests start here.
#include <cstdio>

struct Randoms {
    uint64_t some[4];
    
    Randoms() {
        FILE *fp = fopen("/dev/urandom", "r");
        if (fp == NULL)
            fp = fopen("/dev/random", "r");
        if (fp) {
            fread((void *)some, sizeof(*some), 4, fp);
            fclose(fp);
        }
    }
};

struct JSONParserTests {
    char const *failed = nullptr;
    operator bool() const { return failed == nullptr; }
    
    bool parsed(char const *json) const {
        auto dummy = JSONValueDelegate<void>();
        auto parser = JSONParser(json, &dummy);
        
        parser.parse();
        return !!dummy;
    }
    bool parsed(std::string const &json) const {
        return parsed(json.c_str());
    }
    void testBadJSON1() const {
        try {
            parsed("{");
            throw __FUNCTION__;
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
        }
    }
    void testBadJSON1_1() const {
        try {
            parsed("}");
            throw __FUNCTION__;
        }
        catch (JSONParser::ExpectationFailure const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::ExpectationFailure" << std::endl;
        }
    }
    void testBadJSON2() const {
        try {
            parsed("[");
            throw __FUNCTION__;
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
        }
    }
    void testBadJSON2_1() const {
        try {
            parsed("]");
            throw __FUNCTION__;
        }
        catch (JSONParser::ExpectationFailure const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::Error" << std::endl;
        }
    }
    void testBadJSON2_2() const {
        try {
            parsed("true");
            throw __FUNCTION__;
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
        }
    }
    void testBadJSON2_3() const {
        try {
            parsed("false");
            throw __FUNCTION__;
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
        }
    }
    void testBadJSON2_4() const {
        try {
            parsed("null");
            throw __FUNCTION__;
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
        }
    }
    void testBadJSON2_5() const {
        try {
            parsed("\"foo\"");
            throw __FUNCTION__;
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
        }
    }
    void testBadJSON2_6() const {
        try {
            parsed("adsfa");
            throw __FUNCTION__;
        }
        catch (JSONParser::Unexpected const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::Unexpected" << std::endl;
        }
    }
    void testBadJSON3() const {
        try {
            parsed(",");
            throw __FUNCTION__;
        }
        catch (JSONParser::ExpectationFailure const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::ExpectationFailure" << std::endl;
        }
    }
    void testTortureJSON_1(unsigned &smallest, unsigned &biggest) const {
        std::string torture = "";
        std::string torture2 = "";
        Randoms const randoms;
        
        for (auto i = 0; i < 4; ++i) {
            auto rval = randoms.some[i];
            int depth = 0;
            while (depth < 64) {
                if (rval & 1) {
                    torture.append("{\"m\":");
                    torture2.append(1, '}');
                }
                torture.append(1, '[');
                torture2.append(1, ']');
                rval >>= 1;
                ++depth;
            }
        }
        auto depth = torture2.size();
        smallest = std::min(smallest, (unsigned)depth);
        biggest = std::max(biggest, (unsigned)depth);
        
        torture.append(torture2.rbegin(), torture2.rend());
        assert(parsed(torture));
    }
    void testNestedStructs() const {
        try {
            int i;
            unsigned smallest = 10000, biggest = 0;
            for (i = 0; i < smallest; ++i)
                testTortureJSON_1(smallest, biggest);
            LOG(9) << __FUNCTION__ << " successful, tested " << i << " random nested structures, max depth was " << biggest << std::endl;
            return;
        }
        catch (JSONParser::Error const &e) {
            std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
        }
        catch (...) {
            std::cerr << __FUNCTION__ << " failed" << std::endl;
        }
        throw __FUNCTION__;
    }
    void testEmpty() {
        try {
            parsed("");
            throw __FUNCTION__;
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
        }
    }
    JSONParserTests()
    : failed(nullptr)
    {
        try {
            testBadJSON1();
            testBadJSON1_1();
            testBadJSON2();
            testBadJSON2_1();
            testBadJSON2_2();
            testBadJSON2_3();
            testBadJSON2_4();
            testBadJSON2_5();
            testBadJSON2_6();
            testBadJSON3();
            testNestedStructs();
            testEmpty();
        }
        catch (char const *function) {
            failed = function;
        }
    }
};

void JSONParser::runTests() {
    try {
        JSONParserTests tests;
        if (tests) {
            LOG(8) << "All JSON parsing tests passed." << std::endl;
            return;
        }
        std::cerr << tests.failed << " failed." << std::endl;
    }
    catch (...) {
        std::cerr << __FUNCTION__ << " failed." << std::endl;
    }
    abort();
}

struct UnicharTests {
    char buffer[5];
    
    void encode(Unichar u) {
        buffer[u.utf8(buffer)] = '\0';
    }
    void testEncodingAscii() {
        try {
            encode(Unichar('~'));
            assert(std::string(buffer) == "~");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testEncoding2() {
        try {
            encode(Unichar(0x20AC));
            assert(buffer[0] == '\xE2' && buffer[1] == '\x82' && buffer[2] == '\xAC' && buffer[3] == '\0');
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testEncoding3() {
        try {
            encode(Unichar(0x10348));
            assert(buffer[0] == '\xF0' && buffer[1] == '\x90' && buffer[2] == '\x8D' && buffer[3] == '\x88' && buffer[4] == '\0');
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testMissingSurrogate1() {
        try {
            encode(Unichar{0xDC37});
            throw __FUNCTION__;
        }
        catch (JSONStringConversionInvalidUTF8Error const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONStringConversionInvalidUTF8Error." << std::endl;
        }
    }
    void testMissingSurrogate2() {
        try {
            encode(Unichar{0xD801});
            throw __FUNCTION__;
        }
        catch (JSONStringConversionInvalidUTF8Error const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONStringConversionInvalidUTF8Error." << std::endl;
        }
    }
    void testEncodingTooBig() {
        try {
            encode(Unichar(0x200348));
            throw __FUNCTION__;
        }
        catch (JSONStringConversionInvalidUTF8Error const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONStringConversionInvalidUTF8Error." << std::endl;
        }
    }
    void testDecodingAscii() {
        try {
            Unichar u;
            assert(true == u.add_UTF8('~'));
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecodingUTF8() {
        try {
            Unichar u;
            assert(false == u.add_UTF8('\xE2'));
            assert(false == u.add_UTF8('\x82'));
            assert(true == u.add_UTF8('\xAC'));
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecodingUTF16_1() {
        try {
            Unichar u;
            assert(true == u.add_UTF16(0x20AC));
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecodingUTF16_2() {
        try {
            Unichar u;
            assert(false == u.add_UTF16(0xD801));
            assert(true == u.add_UTF16(0xDC37));
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecodingUTF16_3() {
        try {
            Unichar u;
            assert(false == u.add_UTF16(0xD801));
            assert(true == u.add_UTF16(0xD037));
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testDecodingHex16_1() {
        try {
            Unichar u;
            assert(false == u.add_Hex16('2'));
            assert(false == u.add_Hex16('0'));
            assert(false == u.add_Hex16('A'));
            assert(true == u.add_Hex16('C'));
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecodingHex16_2() {
        try {
            Unichar u;
            assert(false == u.add_Hex16('D'));
            assert(false == u.add_Hex16('8'));
            assert(false == u.add_Hex16('0'));
            assert(false == u.add_Hex16('1'));
            assert(false == u.add_Hex16('D'));
            assert(false == u.add_Hex16('C'));
            assert(false == u.add_Hex16('3'));
            assert(true == u.add_Hex16('7'));
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecodingHex16_3() {
        try {
            Unichar u;
            assert(false == u.add_Hex16('2'));
            assert(false == u.add_Hex16('x'));
            assert(false == u.add_Hex16('A'));
            assert(true == u.add_Hex16('C'));
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testDecodingHex16_4() {
        try {
            Unichar u;
            assert(false == u.add_Hex16('D'));
            assert(false == u.add_Hex16('8'));
            assert(false == u.add_Hex16('0'));
            assert(false == u.add_Hex16('1'));
            assert(false == u.add_Hex16('D'));
            assert(false == u.add_Hex16('0'));
            assert(false == u.add_Hex16('3'));
            assert(true == u.add_Hex16('7'));
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    UnicharTests() {
        testDecodingAscii();
        testDecodingUTF8();
        testDecodingUTF16_1();
        testDecodingUTF16_2();
        testDecodingUTF16_3();
        testDecodingHex16_1();
        testDecodingHex16_2();
        testDecodingHex16_3();
        testDecodingHex16_4();

        LOG(8) << "All Unicode decoding tests passed." << std::endl;

        testEncodingAscii();
        testEncoding2();
        testEncoding3();
        testEncodingTooBig();
        testMissingSurrogate1();
        testMissingSurrogate2();

        LOG(8) << "All Unicode encoding tests passed." << std::endl;
    }
};

void Unichar::runTests() {
    try {
        UnicharTests unicharTests;

        LOG(8) << "All UTF8 encoding tests passed." << std::endl;
        return;
    }
    catch (char const *function) {
        std::cerr << function << " failed." << std::endl;
    }
    abort();
}

struct JSONStringTests {
    static JSONString make(char const *cstr, bool isMemberName = false)
    {
        auto end = cstr;
        while (*end)
            ++end;
        return JSONString(StringView(cstr, end), isMemberName);
    }
    void testAString() {
        try {
            make("\"foo\"");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecodeUTF8_UTF16() {
        try {
            std::string const s1 = make("\"\u20AC\"");
            std::string const s2 = make("\"\xE2\x82\xAC\"");
            assert(s1 == s2);
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecode16String1() {
        try {
            std::string s = make("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u20AC\"");
            assert(s == "\"\\/\b\f\n\r\t\xE2\x82\xAC");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testDecode16String2() {
        try {
            std::string const s = make("\"\\uD801\\uDC37\"");
            assert(s == "\xF0\x90\x90\xB7");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testBigUTF8_1() {
        try {
            std::string const s = make("\"\x7F\"");
            assert(s == "\x7F");

            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testBigUTF8_2() {
        try {
            std::string const s = make("\"\xDF\xBF\"");
            assert(s == "\xDF\xBF");

            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testBigUTF8_3() {
        try {
            std::string const s = make("\"\xEF\xBF\xBF\"");
            assert(s == "\xEF\xBF\xBF");

            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testBigUTF8_4() {
        try {
            std::string const s = make("\"\xF4\x8F\xBF\xBF\"");
            assert(s == "\xF4\x8F\xBF\xBF");

            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testNotAString() {
        try {
            make("foo");
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testTooBigUTF8() {
        try {
            std::string const s = make("\"\xF7\xBF\xBF\xBF\""); // fails is_valid_as_UTF8; line 565
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testTooTooBigUTF8() {
        try {
            std::string const s = make("\"\xFB\xBF\xBF\xBF\xBF\"");
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode1() {
        try {
            std::string const s = make("\"\\v\""); // fails case unrecognized escaped character; line 550
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode1_2() {
        try {
            std::string const s = make("\"\\\""); // fails end of input in escape sequence; line 580
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2_1() {
        try {
            std::string const s = make("\"\\uA\"");
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2_2() {
        try {
            std::string const s = make("\"\\uA0\"");
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2_3() {
        try {
            std::string const s = make("\"\\uA0F\"");
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2_4() {
        try {
            std::string const s = make("\"\\uA0\\F0\""); // fails escape in hex string; line 522
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2() {
        testBadDecode2_1();
        testBadDecode2_2();
        testBadDecode2_3();
        testBadDecode2_4();
    }

    void testBadDecode3() {
        char buffer[8];
        auto const n = UnicharBasic(0xD801).utf8((uint8_t *)(buffer + 1));

        buffer[n + 1] = buffer[0] = '"';
        buffer[n + 2] = '\0';
        try {
            std::string const s = make(buffer); // fails is_valid_as_UTF8; line 565
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadLeadingByte1() {
        char buffer[8];
        buffer[0] = '"';
        auto n = Unichar(0xD8).utf8(buffer + 1);
        assert(n == 2);
        buffer[1] |= 0x20; // set a bit that should be 0
        buffer[++n] = '"';
        buffer[++n] = '\0';
        try {
            std::string const s = make(buffer); // fails end of input while decoding UTF-8; line 578
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadLeadingByte2() {
        char buffer[8];
        buffer[0] = '"';
        auto n = Unichar(0xD81).utf8(buffer + 1);
        assert(n == 3);
        buffer[1] |= 0x10; // set a bit that should be 0
        buffer[++n] = '"';
        buffer[++n] = '\0';
        try {
            std::string const s = make(buffer); // fails end of input while decoding UTF-8; line 578
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadLeadingByte3() {
        char buffer[8];
        buffer[0] = '"';
        auto n = Unichar(0xD8).utf8(buffer + 1);
        assert(n == 2);
        buffer[1] ^= 0x60; // make an invalid leading byte
        buffer[++n] = '"';
        buffer[++n] = '\0';
        try {
            std::string const s = make(buffer);
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadLeadingByte4() {
        char buffer[8];
        buffer[0] = '"';
        auto n = Unichar(0xD81).utf8(buffer + 1);
        assert(n == 3);
        buffer[1] ^= 0x30; // make an invalid leading byte
        buffer[++n] = '"';
        buffer[++n] = '\0';
        try {
            std::string const s = make(buffer);
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadLeadingByte5() {
        char buffer[8];
        buffer[0] = '"';
        auto n = Unichar(0xD81).utf8(buffer + 1);
        assert(n == 3);
        buffer[1] ^= 0x60; // make an invalid leading byte
        buffer[++n] = '"';
        buffer[++n] = '\0';
        try {
            std::string const s = make(buffer);
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadLeadingByte6() {
        char buffer[8];
        buffer[0] = '"';
        auto n = Unichar(0x1D811).utf8(buffer + 1);
        assert(n == 4);
        buffer[1] ^= 0x60; // make an invalid leading byte
        buffer[++n] = '"';
        buffer[++n] = '\0';
        try {
            std::string const s = make(buffer);
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadContinuationByte() {
        char buffer[8];
        buffer[0] = '"';
        auto n = Unichar(0xD81).utf8(buffer + 1);
        buffer[n] |= 0x40; // set bit 6 to make a bad UTF continuation byte
        buffer[++n] = '"';
        buffer[++n] = '\0';
        try {
            std::string const s = make(buffer);
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadMemberNameEmpty() {
        try {
            std::string const s = make("", true);
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }

    JSONStringTests() {
        Unichar::runTests();
        testAString();
        testDecodeUTF8_UTF16();
        testDecode16String1();
        testDecode16String2();
        testBigUTF8_1();
        testBigUTF8_2();
        testBigUTF8_3();
        testBigUTF8_4();
        testNotAString();
        testTooBigUTF8();
        testTooTooBigUTF8();
        testBadDecode1();
        testBadDecode1_2();
        testBadDecode2();
        testBadDecode3();
        testBadLeadingByte1();
        testBadLeadingByte2();
        testBadLeadingByte3();
        testBadLeadingByte4();
        testBadLeadingByte5();
        testBadLeadingByte6();
        testBadContinuationByte();
        testBadMemberNameEmpty();
    }
};

struct JSONMemberNameConstraintsTests {
#if PEDANTIC_MEMBER_NAMES
    static JSONString make(char const *cstr)
    {
        auto end = cstr;
        while (*end)
            ++end;
        return JSONString(StringView(cstr, end), true);
    }
    void testBadMemberNameNumeric() {
        try {
            std::string const s = make("1foo");
            throw __FUNCTION__;
        }
        catch (BadMemberNameError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got BadMemberNameError." << std::endl;
        }
    }
    void testBadMemberName1() {
        try {
            std::string const s = make(".foo");
            throw __FUNCTION__;
        }
        catch (BadMemberNameError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got BadMemberNameError." << std::endl;
        }
    }
    void testBadMemberName2() {
        try {
            std::string const s = make("foo.1");
            throw __FUNCTION__;
        }
        catch (BadMemberNameError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got BadMemberNameError." << std::endl;
        }
    }
    void testMemberName1() {
        try {
            std::string const s = make("foo");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testMemberName2() {
        try {
            std::string const s = make("_foo");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
    void testMemberName3() {
        try {
            std::string const s = make("foo22");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw __FUNCTION__;
        }
    }
#endif
    JSONMemberNameConstraintsTests() {
#if PEDANTIC_MEMBER_NAMES
        testBadMemberNameNumeric();
        testBadMemberName1();
        testBadMemberName2();
        testMemberName1();
        testMemberName2();
        testMemberName3();
#endif
    }
};

void JSONString::runTests() {
    try {
        {
            JSONStringTests tests;

            (void)(tests);
            LOG(8) << "All JSON string decoding tests passed." << std::endl;
        }
        {
            JSONMemberNameConstraintsTests tests;

            (void)(tests);
            LOG(8) << "All JSON member name constraints tests passed." << std::endl;
        }
        return;
    }
    catch (char const *function) {
        std::cerr << function << " failed." << std::endl;
    }
    abort();
}

void JSONBool::runTests() {
    try {
        try {
            auto const T = JSONBool(StringView("foo"));
            (void)(T);
            throw "failed";
        }
        catch (JSONScalarConversionError const &) {
        }
        auto const T = JSONBool(StringView("true"));
        auto const F = JSONBool(StringView("false"));
        
        assert(!F);
        assert(!!T);
        LOG(8) << "All JSON bool tests passed." << std::endl;
        return;
    }
    catch (...) {
        std::cerr << "JSONBoolTests failed." << std::endl;
    }
}

void JSONNull::runTests() {
    try {
        try {
            auto const T = JSONNull(StringView("foo"));
            (void)(T);
            throw "failed";
        }
        catch (JSONScalarConversionError const &) {
        }
        auto const T = JSONNull(StringView("null"));
        (void)(T);

        LOG(8) << "All JSON null tests passed." << std::endl;
        return;
    }
    catch (...) {
        std::cerr << "JSONNullTests failed." << std::endl;
    }
}

#endif
