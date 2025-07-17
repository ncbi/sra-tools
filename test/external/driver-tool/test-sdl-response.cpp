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
 *  Tests for Parsing SDL Response
*
*/

#if WINDOWS
#pragma warning(disable: 4100)
#pragma warning(disable: 4101)
#endif

#include "json-parse.cpp"
#include "SDL-response.cpp"

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

template <typename IMPL, typename RESPONSE = typename IMPL::Base, typename DELEGATE = typename IMPL::Delegate>
static RESPONSE makeFrom(char const *json)
{
    auto delegate = DELEGATE();
    auto topLevel = TopLevelObjectDelegate(&delegate);
    auto parser = JSONParser(json, &topLevel);

    parser.parse();
    return delegate.get(1);
}

/// Parsing and value extraction tests.
struct Response2Tests {
    struct ResultEntryTests {
        struct FileEntryTests {
            struct LocationEntryTests {
                using Impl = impl::Response2::ResultEntry::FileEntry::LocationEntry;
                static Impl::Base makeFrom(char const *json) {
                    return ::makeFrom<Impl>(json);
                }
                void runTests() {
                    emptyObjectTest();
                    missingServiceTest();
                    missingLinkTest();
                    missingRegionTest();
                    defaultedRegionTest();
                    haveExpDateTest();
                    haveProjectTest();
                    haveCERTestFalse();
                    haveCERTestTrue();
                    nullCER();
                    badCER();
                    havePayRTestFalse();
                    havePayRTestTrue();
                }
                void havePayRTestTrue() {
                    try {
                        auto const obj = makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz",
                                "region": "be-md",
                                "payRequired": true
                            }
                            )###");
                        ASSERT(obj.payRequired == true);
                        LOG(9) << __FUNCTION__ << " successful" << std::endl;
                        return;
                    }
                    catch (JSONParser::Error const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                    }
                    catch (JSONScalarConversionError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                    }
                    catch (Response2::DecodingError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                    }
                    throw test_failure(__FUNCTION__);
                }
                void havePayRTestFalse() {
                    try {
                        makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz",
                                "region": "be-md",
                                "payRequired": false
                            }
                            )###");
                        LOG(9) << __FUNCTION__ << " successful" << std::endl;
                        return;
                    }
                    catch (JSONParser::Error const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                    }
                    catch (JSONScalarConversionError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                    }
                    catch (Response2::DecodingError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                    }
                    throw test_failure(__FUNCTION__);
                }
                void badCER() {
                    try {
                        makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz",
                                "region": "be-md",
                                "ceRequired": "foo"
                            }
                            )###");
                        throw test_failure(__FUNCTION__);
                    }
                    catch (JSONScalarConversionError const &e) {
                        LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError" << std::endl;
                    }
                }
                void nullCER() {
                    try {
                        makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz",
                                "region": "be-md",
                                "ceRequired": null
                            }
                            )###");
                        throw test_failure(__FUNCTION__);
                    }
                    catch (JSONScalarConversionError const &e) {
                        LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError" << std::endl;
                    }
                }
                void haveCERTestTrue() {
                    try {
                        auto const obj = makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz",
                                "region": "be-md",
                                "ceRequired": true
                            }
                            )###");
                        ASSERT(obj.ceRequired == true);
                        LOG(9) << __FUNCTION__ << " successful" << std::endl;
                        return;
                    }
                    catch (JSONParser::Error const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                    }
                    catch (JSONScalarConversionError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                    }
                    catch (Response2::DecodingError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                    }
                    throw test_failure(__FUNCTION__);
                }
                void haveCERTestFalse() {
                    try {
                        makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz",
                                "region": "be-md",
                                "ceRequired": false
                            }
                            )###");
                        LOG(9) << __FUNCTION__ << " successful" << std::endl;
                        return;
                    }
                    catch (JSONParser::Error const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                    }
                    catch (JSONScalarConversionError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                    }
                    catch (Response2::DecodingError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                    }
                    throw test_failure(__FUNCTION__);
                }
                void haveProjectTest() {
                    try {
                        auto const obj = makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz",
                                "region": "be-md",
                                "encryptedForProjectId": "phs-1234"
                            }
                            )###");
                        ASSERT(obj.projectId && obj.projectId.value() == "phs-1234");
                        LOG(9) << __FUNCTION__ << " successful" << std::endl;
                        return;
                    }
                    catch (JSONParser::Error const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                    }
                    catch (JSONScalarConversionError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                    }
                    catch (Response2::DecodingError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                    }
                    throw test_failure(__FUNCTION__);
                }
                void haveExpDateTest() {
                    try {
                        auto const obj = makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz",
                                "region": "be-md",
                                "expirationDate": "2012-01-19T20:14:00Z"
                            }
                            )###");
                        ASSERT(obj.expirationDate && obj.expirationDate.value() == "2012-01-19T20:14:00Z");
                        LOG(9) << __FUNCTION__ << " successful" << std::endl;
                        return;
                    }
                    catch (JSONParser::Error const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                    }
                    catch (JSONScalarConversionError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                    }
                    catch (Response2::DecodingError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                    }
                    throw test_failure(__FUNCTION__);
                }
                void defaultedRegionTest() {
                    try {
                        auto const obj = makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz"
                            }
                            )###");
                        ASSERT(obj.region == "be-md");
                        LOG(9) << __FUNCTION__ << " successful" << std::endl;
                        return;
                    }
                    catch (JSONParser::Error const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                    }
                    catch (JSONScalarConversionError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                    }
                    catch (Response2::DecodingError const &e) {
                        std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                    }
                    throw test_failure(__FUNCTION__);
                }
                void missingRegionTest() {
                    try {
                        makeFrom(R"###(
                            {
                                "service": "sra-ncbi",
                                "link": "https://foo.bar.baz"
                            }
                            )###");
                        throw test_failure(__FUNCTION__);
                    }
                    catch (Response2::DecodingError const &e) {
                        LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                    }
                }
                void missingLinkTest() {
                    try {
                        makeFrom(
                            R"###( { "service": "sra-ncbi" } )###"
                        );
                        throw test_failure(__FUNCTION__);
                    }
                    catch (Response2::DecodingError const &e) {
                        LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                    }
                }
                void missingServiceTest() {
                    try {
                        makeFrom(R"###( { "link": "https" } )###");
                        throw test_failure(__FUNCTION__);
                    }
                    catch (Response2::DecodingError const &e) {
                        LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                    }
                }
                void emptyObjectTest() {
                    try {
                        makeFrom("{}");
                        throw test_failure(__FUNCTION__);
                    }
                    catch (Response2::DecodingError const &e) {
                        LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                    }
                }
            } child;
            using Impl = impl::Response2::ResultEntry::FileEntry;
            static Impl::Base makeFrom(char const *json) {
                return ::makeFrom<Impl>(json);
            }
            void runTests() {
                child.runTests();
                emptyObjectTest();
                missingNameTest();
                missingTypeTest();
                noDefaultTypeTest();
                emptyLocationsTest();
                defaultedTypeTest();
                haveSizeTest();
                haveMD5Test();
                haveModDateTest();
                haveNoQualTest();
                haveFormatTest();
            }
            void haveFormatTest() {
                try {
                    auto const obj = makeFrom(R"###(
                        {
                            "object": "srapub_files|SRR850901",
                            "type": "reference_fasta",
                            "name": "SRR850901.contigs.fasta",
                            "format": "text/fasta"
                        }
                        )###");
                    ASSERT(obj.format && obj.format.value() == "text/fasta");
                    LOG(9) << __FUNCTION__ << " successful" << std::endl;
                    return;
                }
                catch (JSONParser::Error const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                }
                catch (JSONScalarConversionError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                }
                catch (Response2::DecodingError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                }
                throw test_failure(__FUNCTION__);
            }
            void haveNoQualTest() {
                try {
                    auto const obj = makeFrom(R"###(
                        {
                            "object": "srapub_files|SRR850901",
                            "type": "sra",
                            "name": "SRR850901.pileup",
                            "noqual": true
                        }
                        )###");
                    ASSERT(obj.noqual == true);
                    LOG(9) << __FUNCTION__ << " successful" << std::endl;
                    return;
                }
                catch (JSONParser::Error const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                }
                catch (JSONScalarConversionError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                }
                catch (Response2::DecodingError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                }
                throw test_failure(__FUNCTION__);
            }
            void haveModDateTest() {
                try {
                    auto const obj = makeFrom(R"###(
                        {
                            "object": "srapub_files|SRR850901",
                            "type": "sra",
                            "name": "SRR850901.pileup",
                            "modificationDate": "2016-11-20T07:38:19Z"
                        }
                        )###");
                    ASSERT(obj.modificationDate && obj.modificationDate.value() == "2016-11-20T07:38:19Z");
                    LOG(9) << __FUNCTION__ << " successful" << std::endl;
                    return;
                }
                catch (JSONParser::Error const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                }
                catch (JSONScalarConversionError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                }
                catch (Response2::DecodingError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                }
                throw test_failure(__FUNCTION__);
            }
            void haveMD5Test() {
                try {
                    auto const obj = makeFrom(R"###(
                        {
                            "object": "srapub_files|SRR850901",
                            "type": "sra",
                            "name": "SRR850901.pileup",
                            "md5": "323d0b76f741ae0b71aeec0176645301"
                        }
                        )###");
                    ASSERT(obj.md5 && obj.md5.value() == "323d0b76f741ae0b71aeec0176645301");
                    LOG(9) << __FUNCTION__ << " successful" << std::endl;
                    return;
                }
                catch (JSONParser::Error const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                }
                catch (JSONScalarConversionError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                }
                catch (Response2::DecodingError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                }
                throw test_failure(__FUNCTION__);
            }
            void haveSizeTest() {
                try {
                    auto const obj = makeFrom(R"###(
                        {
                            "object": "srapub_files|SRR850901",
                            "type": "sra",
                            "name": "SRR850901.pileup",
                            "size": 842809
                        }
                        )###");
                    ASSERT(obj.size && obj.size.value() == "842809");
                    LOG(9) << __FUNCTION__ << " successful" << std::endl;
                    return;
                }
                catch (JSONParser::Error const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                }
                catch (JSONScalarConversionError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                }
                catch (Response2::DecodingError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                }
                throw test_failure(__FUNCTION__);
            }
            void defaultedTypeTest() {
                try {
                    auto const obj = makeFrom(R"###(
                        {
                            "object": "wgs|AAAA00",
                            "name": "AAAA02.11"
                        }
                        )###");
                    ASSERT(obj.type == "sra");
                    LOG(9) << __FUNCTION__ << " successful" << std::endl;
                    return;
                }
                catch (JSONParser::Error const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                }
                catch (JSONScalarConversionError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                }
                catch (Response2::DecodingError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                }
                throw test_failure(__FUNCTION__);
            }
            void emptyLocationsTest() {
                try {
                    auto const obj = makeFrom(R"###(
                        {
                            "object": "wgs|AAAA00",
                            "name": "AAAA02.11",
                            "locations": []
                        }
                        )###");
                    ASSERT(obj.locations.empty());
                    LOG(9) << __FUNCTION__ << " successful" << std::endl;
                    return;
                }
                catch (JSONParser::Error const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
                }
                catch (JSONScalarConversionError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
                }
                catch (Response2::DecodingError const &e) {
                    std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
                }
                throw test_failure(__FUNCTION__);
            }
            void noDefaultTypeTest() {
                try {
                    makeFrom(R"###( {
                        "name": "SRR000002",
                        "object": "srapub|SRR000002"
                    } )###");
                    throw test_failure(__FUNCTION__);
                }
                catch (Response2::DecodingError const &e) {
                    LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                }
            }
            void missingTypeTest() {
                try {
                    makeFrom(R"###( { "name": "SRR000002" } )###");
                    throw test_failure(__FUNCTION__);
                }
                catch (Response2::DecodingError const &e) {
                    LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                }
            }
            void missingNameTest() {
                try {
                    makeFrom(R"###( { "type": "sra" } )###");
                    throw test_failure(__FUNCTION__);
                }
                catch (Response2::DecodingError const &e) {
                    LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                }
            }
            void emptyObjectTest() {
                try {
                    makeFrom("{}");
                    throw test_failure(__FUNCTION__);
                }
                catch (Response2::DecodingError const &e) {
                    LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                }
            }
        } child;
        using Impl = impl::Response2::ResultEntry;
        static Impl::Base makeFrom(char const *json) {
            return ::makeFrom<Impl>(json);
        }
        void runTests() {
            child.runTests();
            emptyObjectTest();
            missingStatusTest();
            missingBundleTest();
            missingMessageTest();
            test404Response();
        }
        void test404Response() {
            try {
                auto const obj = makeFrom(R"###(
                    {
                        "bundle": "SRR867664",
                        "status": 404,
                        "msg": "No data at given location.region"
                    }
                    )###");
                ASSERT(obj.query == "SRR867664");
                ASSERT(obj.status == "404");
                ASSERT(obj.message == "No data at given location.region");
                LOG(9) << __FUNCTION__ << " successful" << std::endl;
                return;
            }
            catch (JSONParser::Error const &e) {
                std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
            }
            catch (JSONScalarConversionError const &e) {
                std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
            }
            catch (Response2::DecodingError const &e) {
                std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
            }
            throw test_failure(__FUNCTION__);
        }
        void missingMessageTest() {
            try {
                makeFrom(R"###(
                    {
                        "status": 200,
                        "bundle": "foo"
                    }
                )###");
                throw test_failure(__FUNCTION__);
            }
            catch (Response2::DecodingError const &e) {
                LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
            }
        }
        void missingBundleTest() {
            try {
                makeFrom(R"###(
                    {
                        "status": 200,
                        "msg": "Hello World"
                    }
                )###");
                throw test_failure(__FUNCTION__);
            }
            catch (Response2::DecodingError const &e) {
                LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
            }
        }
        void missingStatusTest() {
            try {
                makeFrom(R"###(
                    {
                        "msg": "Hello World",
                        "bundle": "foo"
                    }
                )###");
                throw test_failure(__FUNCTION__);
            }
            catch (Response2::DecodingError const &e) {
                LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
            }
        }
        void emptyObjectTest() {
            try {
                makeFrom("{}");
                throw test_failure(__FUNCTION__);
            }
            catch (Response2::DecodingError const &e) {
                LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
            }
        }
    } child;

    bool passed;
    Response2Tests()
    : passed(false)
    {
        // These are expected to throw.
        try {
            child.runTests();
            testBadVersion();
            testBadResult();
            testEmpty2();
            testBadVersion2();
            testErrorResponse();
            testGoodResponse();
            testNoResult();

            passed = true;
            LOG(8) << "All SDL response parsing tests passed." << std::endl;
            return;
        }
        catch (char const *function) {
            std::cerr << function << " failed." << std::endl;
            throw test_failure(function);
        }
    }
    static void testEmpty2() {
        try {
            Response2::makeFrom("{}");
            throw test_failure(__FUNCTION__);
        }
        catch (Response2::DecodingError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
        }
    }
    static void testBadVersion() {
        // version is supposed to be a string value.
        try {
            Response2::makeFrom(R"###({ "version": 99 })###");
            throw test_failure(__FUNCTION__);
        }
        catch (JSONScalarConversionError const &e) {
            LOG(9) << __FUNCTION__ << " successful, got: JSONScalarConversionError" << std::endl;
        }
    }
    static void testBadVersion2() {
        // version is supposed to "2" or "2.x".
        try {
            Response2::makeFrom(R"###({ "version": "99" })###");
            throw test_failure(__FUNCTION__);
        }
        catch (Response2::DecodingError const &e) {
            ASSERT(e.object == Response2::DecodingError::topLevel);
            ASSERT(e.haveCause("version"));
            LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
        }
    }
    static void testNoResult() {
        try {
            Response2::makeFrom(R"###({ "version": "2" })###");
            LOG(9) << __FUNCTION__ << " successful" << std::endl;
            return;
        }
        catch (JSONParser::Error const &e) {
            std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
        }
        catch (JSONScalarConversionError const &e) {
            std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
        }
        catch (Response2::DecodingError const &e) {
            std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
        }
        throw test_failure(__FUNCTION__);
    }
    static void testBadResult() {
        try {
            Response2::makeFrom(
                R"###({
                    "version": "2",
                    "result": [{}]
                })###");
            throw test_failure(__FUNCTION__);
        }
        catch (Response2::DecodingError const &e) {
            ASSERT(e.object == Response2::DecodingError::result);
            LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
        }
    }
    static void testErrorResponse() {
        try {
            auto const response = Response2::makeFrom(
                R"###({
                    "version": "2.1",
                    "status": 500,
                    "message": "Internal Server Error"
                })###");
            ASSERT(response.status == "500" && response.message == "Internal Server Error");
            LOG(9) << __FUNCTION__ << " successful" << std::endl;
            return;
        }
        catch (JSONParser::Error const &e) {
            std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
        }
        catch (JSONScalarConversionError const &e) {
            std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
        }
        catch (Response2::DecodingError const &e) {
            std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
        }
        throw test_failure(__FUNCTION__);
    }
    static void testGoodResponse() {
        try {
            Response2::makeFrom(
                R"###({
                    "version": "2",
                    "result": [
                        {
                            "bundle": "SRR123456",
                            "status": 404,
                            "msg": "Not found"
                        }
                    ]
                })###");
            LOG(9) << __FUNCTION__ << " successful" << std::endl;
            return;
        }
        catch (JSONParser::Error const &e) {
            std::cerr << __FUNCTION__ << " failed, got: JSONParser::Error" << std::endl;
        }
        catch (JSONScalarConversionError const &e) {
            std::cerr << __FUNCTION__ << " failed, got: JSONScalarConversionError" << std::endl;
        }
        catch (Response2::DecodingError const &e) {
            std::cerr << __FUNCTION__ << " failed, got: " << e << std::endl;
        }
        throw test_failure(__FUNCTION__);
    }
};

static bool test_parsing() {
    auto const response2Tests = Response2Tests{};
    return response2Tests.passed;
}

static void test_matching_vdbcache() {
    auto const testJSON = R"###(
{
    "version": "2",
    "result": [
        {
            "bundle": "SRR850901",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub_files|SRR850901",
                    "type": "reference_fasta",
                    "name": "SRR850901.contigs.fasta",
                    "size": 5417080,
                    "md5": "e0572326534bbf310c0ac744bd51c1ec",
                    "locations": [
                        {
                            "link": "https://sra-download.ncbi.nlm.nih.gov/traces/sra7/SRZ/000850/SRR850901/SRR850901.contigs.fasta",
                            "service": "sra-ncbi",
                            "region": "public"
                        }
                    ],
                    "modificationDate": "2016-11-19T08:00:46Z"
                },
                {
                    "object": "srapub_files|SRR850901",
                    "type": "sra",
                    "name": "SRR850901.pileup",
                    "size": 842809,
                    "md5": "323d0b76f741ae0b71aeec0176645301",
                    "modificationDate": "2016-11-20T07:38:19Z",
                    "locations": [
                        {
                            "link": "https://sra-download.ncbi.nlm.nih.gov/traces/sra14/SRZ/000850/SRR850901/SRR850901.pileup",
                            "service": "sra-ncbi",
                            "region": "public"
                        }
                    ]
                },
                {
                    "object": "srapub|SRR850901",
                    "type": "sra",
                    "name": "SRR850901",
                    "size": 323741972,
                    "md5": "5e213b2319bd1af17c47120ee8b16dbc",
                    "modificationDate": "2016-11-19T07:35:20Z",
                    "locations": [
                        {
                            "link": "https://sra-downloadb.be-md.ncbi.nlm.nih.gov/sos/sra-pub-run-2/SRR850901/SRR850901.2",
                            "service": "ncbi",
                            "region": "be-md"
                        },
                        {
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR850901/SRR850901",
                            "service": "s3",
                            "region": "us-east-1"
                        }
                    ]
                },
                {
                    "object": "srapub|SRR850901",
                    "type": "vdbcache",
                    "name": "SRR850901.vdbcache",
                    "size": 17615526,
                    "md5": "2eb204cedf5eefe45819c228817ee466",
                    "modificationDate": "2016-02-08T16:31:35Z",
                    "locations": [
                        {
                            "link": "https://sra-download.ncbi.nlm.nih.gov/traces/sra11/SRR/000830/SRR850901.vdbcache",
                            "service": "sra-ncbi",
                            "region": "public"
                        },
                        {
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR850901/SRR850901.vdbcache",
                            "service": "s3",
                            "region": "us-east-1"
                        }
                    ]
                }
            ]
        }
    ]
}
)###";
    try {
        auto const &raw = Response2::makeFrom(testJSON);

        ASSERT(raw.results.size() == 1);
        auto const &result = raw.results[0];

        ASSERT(result.files.size() == 4);
        auto sras = 0;
        auto locations = 0;

        for (auto const &fl : result.getByType("sra")) {
            auto const &file = fl.first;

            ASSERT(file.type == "sra");
            ++sras;

            if (file.hasExtension(".pileup"))
                continue;
            ++locations;

            auto const vcache = result.getCacheFor(fl);
            ASSERT(vcache >= 0);

            auto const &vdbcache = result.flat(vcache).second;
            if (fl.second.service == "ncbi" || fl.second.service == "sra-ncbi") {
                ASSERT(vdbcache.service == fl.second.service || vdbcache.service == "ncbi" || vdbcache.service == "sra-ncbi");
            }
            else {
                ASSERT(vdbcache.service == fl.second.service);
                ASSERT(vdbcache.region == fl.second.region);
            }
        }
        ASSERT(sras == 3); // 2 files and 3 locations
        ASSERT(locations == 2);

        LOG(8) << "vdbcache location matching passed." << std::endl;
    }
    catch (Response2::DecodingError const &e) {
        std::cerr << e << std::endl;
        throw test_failure(__FUNCTION__);
    }
}

static void test_nonmatching_vdbcache() {
    auto const testJSON = R"###(
{
    "version": "2",
    "result": [
        {
            "bundle": "SRR8117283",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR8117283.lite",
                    "accession": "SRR8117283",
                    "type": "sra",
                    "name": "SRR8117283.lite",
                    "size": 10626650910,
                    "md5": "9a44fbc53c2955df8c4f57ecfbab779d",
                    "modificationDate": "2022-10-20T11:09:01Z",
                    "noqual": true,
                    "locations": [
                        {
                            "service": "ncbi",
                            "region": "be-md",
                            "link": "https://sra-download-internal.ncbi.nlm.nih.gov/sos1/sra-pub-zq-38/SRR008/8117/SRR8117283/SRR8117283.lite.2"
                        }
                    ]
                },
                {
                    "object": "srapub|SRR8117283",
                    "accession": "SRR8117283",
                    "type": "vdbcache",
                    "name": "SRR8117283.vdbcache",
                    "size": 1590373098,
                    "md5": "67ac753e225d2b72d62671339598202f",
                    "modificationDate": "2018-11-09T22:59:37Z",
                    "locations": [
                        {
                            "service": "ncbi",
                            "region": "be-md",
                            "link": "https://sra-download-internal.ncbi.nlm.nih.gov/sos5/sra-pub-run-32/SRR008/8117/SRR8117283/SRR8117283.vdbcache.1"
                        }
                    ]
                }
            ]
        }
    ]
}
)###";
    try {
        auto const &raw = Response2::makeFrom(testJSON);

        ASSERT(raw.results.size() == 1);
        auto passed = false;
        auto const files = raw.results[0].files.size();

        ASSERT(files == 2);
        for (auto const &fl : raw.results[0].getByType("sra")) {
            auto const &file = fl.first;
            auto const vcache = raw.results[0].getCacheFor(fl);

            ASSERT(vcache < 0);
            passed = true;
			(void)file;
        }
        ASSERT(passed);

        LOG(8) << "vdbcache location non-matching passed." << std::endl;
    }
    catch (Response2::DecodingError const &e) {
        std::cerr << e << std::endl;
        throw test_failure(__FUNCTION__);
    }
}

static void test_multiple_locations() {
    auto const testJSON = R"###(
{
    "version": "2",
    "result": [
        {
            "bundle": "SRR850901",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR850901",
                    "type": "sra",
                    "name": "SRR850901",
                    "size": 323741972,
                    "md5": "5e213b2319bd1af17c47120ee8b16dbc",
                    "modificationDate": "2016-11-19T07:35:20Z",
                    "locations": [
                        {
                            "link": "https://sra-downloadb.be-md.ncbi.nlm.nih.gov/sos/sra-pub-run-2/SRR850901/SRR850901.2",
                            "service": "ncbi",
                            "region": "be-md"
                        },
                        {
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR850901/SRR850901",
                            "service": "s3",
                            "region": "us-east-1"
                        }
                    ]
                }
            ]
        }
    ]
}
)###";
    try {
        auto const &raw = Response2::makeFrom(testJSON);

        ASSERT(raw.results.size() == 1);
        auto const &result = raw.results[0];

        auto passed = false;
        auto const files = result.files.size();
        auto locations = 0;

        for (auto const &fl : result.getByType("sra")) {
            auto const &file = fl.first;

            ASSERT(file.type == "sra");
            ++locations;

            passed = true;
        }
        ASSERT(files == 1);
        ASSERT(locations == 2);
        ASSERT(passed);

        LOG(8) << "multiple location matching passed." << std::endl;
    }
    catch (Response2::DecodingError const &e) {
        std::cerr << e << std::endl;
        throw test_failure(__FUNCTION__);
    }
}

int main ( int argc, char *argv[], char *envp[])
{
    try {
        if (test_parsing()) {
            test_multiple_locations();
            test_matching_vdbcache();
            test_nonmatching_vdbcache();
            return 0;
        }
    }
    catch (test_failure const &e) {
        std::cerr << "test " << e.what() << " failed." << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 3;
}
