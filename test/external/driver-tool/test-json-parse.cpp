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
 *  Tests for Parsing SDL Response JSON
*
*/

#if WINDOWS
#pragma warning(disable: 4100)
#pragma warning(disable: 4101)
#endif

#include <random>
#include <type_traits>
#include <initializer_list>
#include <algorithm>
#include <cassert>

struct UnicharTests;

#include "json-parse.cpp"

#define IGNORE(X) do { (void)(X); } while (0)

struct test_failure: public std::exception
{
    char const *test_name;
    test_failure(char const *test) : test_name(test) {}
    char const *what() const throw() { return test_name; }
};

struct assertion_failure: public std::exception
{
    std::string message;
    assertion_failure(char const *expr, char const *function, int line)
    {
        message = std::string(__FILE__) + ":" + std::to_string(line) + " in function " + function + " assertion failed: " + expr;
    }
    char const *what() const throw() { return message.c_str(); }
};

#define S_(X) #X
#define S(X) S_(X)
#define ASSERT(X) do { if (X) break; throw assertion_failure(#X, __FUNCTION__, __LINE__); } while (0)

using BitsEngine = std::independent_bits_engine<std::default_random_engine, 64, uint64_t>;
static BitsEngine bitsEngine() {
    static std::random_device randev;
    return BitsEngine(std::default_random_engine(randev()));
}

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
    /// Unbalanced braces: no close
    void testBadJSON1() const {
        try {
            parsed("{");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
            IGNORE(e);
        }
    }
    /// Unbalanced braces: no open
    void testBadJSON1_1() const {
        try {
            parsed("}");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::ExpectationFailure const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::ExpectationFailure" << std::endl;
            IGNORE(e);
        }
    }
    /// Unbalanced brackets: no close
    void testBadJSON2() const {
        try {
            parsed("[");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
            IGNORE(e);
        }
    }
    /// Unbalanced brackets: no open
    void testBadJSON2_1() const {
        try {
            parsed("]");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::ExpectationFailure const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::Error" << std::endl;
            IGNORE(e);
        }
    }
    /// 'true' is a good value, but not top level JSON.
    void testBadJSON2_2() const {
        try {
            parsed("true");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
            IGNORE(e);
        }
    }
    /// 'false' is a good value, but not top level JSON
    void testBadJSON2_3() const {
        try {
            parsed("false");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
            IGNORE(e);
        }
    }
    /// 'null' is a good value, but not top level JSON
    void testBadJSON2_4() const {
        try {
            parsed("null");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
            IGNORE(e);
        }
    }
    /// '"foo"' is a good value, but not top level JSON
    void testBadJSON2_5() const {
        try {
            parsed("\"foo\"");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
            IGNORE(e);
        }
    }
    /// 'adsfa' is **not** a good value
    void testBadJSON2_6() const {
        try {
            parsed("adsfa");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::Unexpected const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::Unexpected" << std::endl;
            IGNORE(e);
        }
    }
    /// ',' is **not** a good value
    void testBadJSON3() const {
        try {
            parsed(",");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::ExpectationFailure const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::ExpectationFailure" << std::endl;
            IGNORE(e);
        }
    }
    /// build a random deeply nested JSON structure
    void testTortureJSON_1(unsigned &smallest, unsigned &biggest) const {
        auto some_bits = bitsEngine();
        std::string torture = ""; // holds the opening symbols
        std::string torture2 = ""; // holds the closing symbols

        for (auto i = 0; i < 4; ++i) {
            auto rval = some_bits();
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

        auto const did_parse = parsed(torture);
        ASSERT(did_parse);
    }
    /// test deeply nested structures
    void testNestedStructs() const {
        try {
            unsigned i;
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
        throw test_failure(__FUNCTION__);
    }
    /// empty is not top level JSON
    void testEmpty() {
        try {
            parsed("");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONParser::EndOfInput const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONParser::EndOfInput" << std::endl;
        }
    }
    JSONParserTests()
    : failed("failed")
    {
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
        testEmpty();
        testNestedStructs();

        failed = nullptr;
    }
};

static void JSONParser_runTests() {
    JSONParserTests tests;
    IGNORE(tests);

    LOG(8) << "All JSON parsing tests passed." << std::endl;
}

struct UnicharTests {
    char buffer[5];

    void encode(Unichar u) {
        char buffer[16];
        u.utf8(buffer);
    }
    void testEncode(Unichar const testValue, std::initializer_list<char> const &expected = {}, char const *const function = nullptr)
    {
        char buffer[16];
        auto const expectedSize = expected.size();
        buffer[testValue.utf8(buffer)] = '\0';

        assert(function != nullptr && expectedSize > 0);
        if (buffer[expectedSize] == '\0' &&
            std::lexicographical_compare(expected.begin(), expected.end(), buffer, buffer + expectedSize) == false &&
            std::lexicographical_compare(buffer, buffer + expectedSize, expected.begin(), expected.end()) == false)
        {
            LOG(9) << function << " successful." << std::endl;
            return;
        }
        throw test_failure(function);
    }
    void testDecode_UTF8(std::initializer_list<char> const &in, Unichar const expected = Unichar{0}, char const *const function = nullptr)
    {
        Unichar u;
        auto cur = in.begin();
        while (cur != in.end() && u.add_UTF8(*cur++) == false)
            ;
        assert(function != nullptr);
        if (cur == in.end() && u == expected)
            return;
        throw test_failure(function);
    }
    void testDecode_UTF16(std::initializer_list<uint16_t> const &in, Unichar const expected = Unichar{0}, char const *const function = nullptr)
    {
        Unichar u;
        auto cur = in.begin();
        while (cur != in.end() && u.add_UTF16(*cur++) == false)
            ;
        assert(function != nullptr);
        if (cur == in.end() && u == expected)
            return;
        throw test_failure(function);
    }
    void testDecode_Hex16(std::initializer_list<char> const &in, Unichar const expected = Unichar{0}, char const *const function = nullptr)
    {
        Unichar u;
        auto cur = in.begin();
        while (cur != in.end() && u.add_Hex16(*cur++) == false)
            ;
        assert(function != nullptr);
        if (cur == in.end() && u == expected)
            return;
        throw test_failure(function);
    }
    /// encodes to one byte
    void testEncodingAscii() {
        testEncode(Unichar{'~'}, {'~'}, __FUNCTION__);
    }
    /// encodes to three bytes
    void testEncoding2() {
        testEncode(Unichar{0x20AC}, {'\xE2', '\x82', '\xAC'}, __FUNCTION__);
    }
    /// encodes to four bytes
    void testEncoding3() {
        testEncode(Unichar{0x10348}, {'\xF0', '\x90', '\x8D', '\x88'}, __FUNCTION__);
    }
    void testMissingSurrogate1() {
        try {
            testEncode(Unichar{0xDC37});
            throw test_failure(__FUNCTION__);
        }
        catch (JSONStringConversionInvalidUTF8Error const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONStringConversionInvalidUTF8Error." << std::endl;
        }
    }
    void testMissingSurrogate2() {
        try {
            testEncode(Unichar{0xD801});
            throw test_failure(__FUNCTION__);
        }
        catch (JSONStringConversionInvalidUTF8Error const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONStringConversionInvalidUTF8Error." << std::endl;
        }
    }
    void testEncodingTooBig() {
        try {
            testEncode(Unichar(0x200348));
            throw test_failure(__FUNCTION__);
        }
        catch (JSONStringConversionInvalidUTF8Error const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONStringConversionInvalidUTF8Error." << std::endl;
        }
    }
    void testDecodingAscii() {
        testDecode_UTF8({'~'}, Unichar{'~'}, __FUNCTION__);
    }
    void testDecodingUTF8() {
        testDecode_UTF8({'\xE2', '\x82', '\xAC'}, Unichar{0x20AC}, __FUNCTION__);
    }
    void testDecodingUTF16_1() {
        testDecode_UTF16({0x20AC}, Unichar{0x20AC}, __FUNCTION__);
    }
    void testDecodingUTF16_2() {
        testDecode_UTF16({0xD801, 0xDC37}, Unichar{0x10437}, __FUNCTION__);
    }
    void testDecodingUTF16_3() {
        try {
            testDecode_UTF16({0xD801, 0xD037}); ///< expected to throw
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testDecodingHex16_1() {
        testDecode_Hex16({'2', '0', 'A', 'C'}, Unichar{0x20AC}, __FUNCTION__);
    }
    void testDecodingHex16_2() {
        testDecode_Hex16({'D', '8', '0', '1', 'D', 'C', '3', '7'}, Unichar{0x10437}, __FUNCTION__);
    }
    void testDecodingHex16_3() {
        try {
            testDecode_Hex16({'2', 'x', 'A', 'C'}); ///< expected to throw
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testDecodingHex16_4() {
        try {
            testDecode_Hex16({'D', '8', '0', '1', 'D', '0', '3', '7'}); ///< expected to throw
            throw test_failure(__FUNCTION__);
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

static void Unichar_runTests() {
    UnicharTests unicharTests;

    LOG(8) << "All UTF8 encoding tests passed." << std::endl;
    return;
}

struct JSONStringTests {
    static JSONString make(char const *cstr, bool isMemberName = false)
    {
        return JSONString(cstr, isMemberName);
    }
    void testAString() {
        try {
            make("\"foo\"");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
            return;
        }
        catch (...) {}
        throw test_failure(__FUNCTION__);
    }
    void testDecodeUTF8_UTF16() {
        try {
            std::string const s1 = make(u8"\"\u20AC\"");
            std::string const s2 = make("\"\xE2\x82\xAC\"");
            if (s1 == s2) {
                LOG(9) << __FUNCTION__ << " successful." << std::endl;
                return;
            }
        }
        catch (...) {}
        throw test_failure(__FUNCTION__);
    }
    void testDecode16String1() {
        try {
            std::string s = make(u8"\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u20AC\"");
            if (s == "\"\\/\b\f\n\r\t\xE2\x82\xAC") {
                LOG(9) << __FUNCTION__ << " successful." << std::endl;
                return;
            }
        }
        catch (...) {}
        throw test_failure(__FUNCTION__);
    }
    void testDecode16String2() {
        try {
            std::string const s = make(u8"\"\\uD801\\uDC37\"");
            if (s == "\xF0\x90\x90\xB7") {
                LOG(9) << __FUNCTION__ << " successful." << std::endl;
                return;
            }
        }
        catch (...) {}
        throw test_failure(__FUNCTION__);
    }
    void testBigUTF8_1() {
        try {
            std::string const s = make("\"\x7F\"");
            if (s == "\x7F") {
                LOG(9) << __FUNCTION__ << " successful." << std::endl;
                return;
            }
        }
        catch (...) {}
        throw test_failure(__FUNCTION__);
    }
    void testBigUTF8_2() {
        try {
            std::string const s = make("\"\xDF\xBF\"");
            if (s == "\xDF\xBF") {
                LOG(9) << __FUNCTION__ << " successful." << std::endl;
                return;
            }
        }
        catch (...) {}
        throw test_failure(__FUNCTION__);
    }
    void testBigUTF8_3() {
        try {
            std::string const s = make("\"\xEF\xBF\xBF\"");
            if (s == "\xEF\xBF\xBF") {
                LOG(9) << __FUNCTION__ << " successful." << std::endl;
                return;
            }
        }
        catch (...) {}
        throw test_failure(__FUNCTION__);
    }
    void testBigUTF8_4() {
        try {
            std::string const s = make("\"\xF4\x8F\xBF\xBF\"");
            if (s == "\xF4\x8F\xBF\xBF") {
                LOG(9) << __FUNCTION__ << " successful." << std::endl;
                return;
            }
        }
        catch (...) {}
        throw test_failure(__FUNCTION__);
    }
    void testNotAString() {
        try {
            make("foo");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testTooBigUTF8() {
        try {
            std::string const s = make("\"\xF7\xBF\xBF\xBF\""); // fails is_valid_as_UTF8; line 565
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testTooTooBigUTF8() {
        try {
            std::string const s = make("\"\xFB\xBF\xBF\xBF\xBF\"");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode1() {
        try {
            std::string const s = make("\"\\v\""); // fails case unrecognized escaped character; line 550
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode1_2() {
        try {
            std::string const s = make("\"\\\""); // fails end of input in escape sequence; line 580
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2_1() {
        try {
            std::string const s = make("\"\\uA\"");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2_2() {
        try {
            std::string const s = make("\"\\uA0\"");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2_3() {
        try {
            std::string const s = make("\"\\uA0F\"");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadDecode2_4() {
        try {
            std::string const s = make("\"\\uA0\\F0\""); // fails escape in hex string; line 522
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }
    void testBadMemberNameEmpty() {
        try {
            std::string const s = make("", true);
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError." << std::endl;
        }
    }

    JSONStringTests() {
        Unichar_runTests();
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
        return JSONString(cstr, true);
    }
    void testBadMemberNameNumeric() {
        try {
            std::string const s = make("1foo");
            throw test_failure(__FUNCTION__);
        }
        catch (BadMemberNameError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got BadMemberNameError." << std::endl;
        }
    }
    void testBadMemberName1() {
        try {
            std::string const s = make(".foo");
            throw test_failure(__FUNCTION__);
        }
        catch (BadMemberNameError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got BadMemberNameError." << std::endl;
        }
    }
    void testBadMemberName2() {
        try {
            std::string const s = make("foo.1");
            throw test_failure(__FUNCTION__);
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
            throw test_failure(__FUNCTION__);
        }
    }
    void testMemberName2() {
        try {
            std::string const s = make("_foo");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw test_failure(__FUNCTION__);
        }
    }
    void testMemberName3() {
        try {
            std::string const s = make("foo22");
            LOG(9) << __FUNCTION__ << " successful." << std::endl;
        }
        catch (...) {
            throw test_failure(__FUNCTION__);
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

static void JSONString_runTests() {
    {
        JSONStringTests tests;

        IGNORE(tests);
        LOG(8) << "All JSON string decoding tests passed." << std::endl;
    }
    {
        JSONMemberNameConstraintsTests tests;

        IGNORE(tests);
        LOG(8) << "All JSON member name constraints tests passed." << std::endl;
    }
}

static void JSONBool_runTests() {
    bool failed = true;
    try {
        auto const T = JSONBool(StringView("foo"));
        IGNORE(T);
    }
    catch (JSONScalarConversionError const &e) {
        failed = false;
        IGNORE(e);
    }
    if (failed) {
        throw test_failure(__FUNCTION__);
    }

    failed = true;
    {
        auto const T = JSONBool(StringView("true"));
        if (!!T)
            failed = false;
    }
    if (failed) {
        throw test_failure(__FUNCTION__);
    }

    failed = true;
    {
        auto const F = JSONBool(StringView("false"));
        if (!F)
            failed = false;
    }
    if (failed) {
        throw test_failure(__FUNCTION__);
    }
    LOG(8) << "All JSON bool tests passed." << std::endl;
}

static void JSONNull_runTests() {
    bool failed = true;
    try {
        auto const T = JSONNull(StringView("foo"));
        IGNORE(T);
    }
    catch (JSONScalarConversionError const &e) {
        failed = false;
        IGNORE(e);
    }
    if (failed) {
        throw test_failure(__FUNCTION__);
    }
    {
        auto const T = JSONNull(StringView("null"));
        IGNORE(T);
    }
    LOG(8) << "All JSON null tests passed." << std::endl;
}

int main ( int argc, char *argv[], char *envp[])
{
    try {
        JSONBool_runTests();
        JSONNull_runTests();
        JSONString_runTests();
        JSONParser_runTests();
        return 0;
    }
    catch (test_failure const &e) {
        std::cerr << "test " << e.what() << " failed." << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 3;
}
