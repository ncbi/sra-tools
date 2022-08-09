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

#include "json-parse.cpp"
#include "SDL-response.cpp"

#if !WINDOWS

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
                        assert(obj.payRequired == true);
                        DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                    catch (...) {
                        std::cerr << __FUNCTION__ << " failed" << std::endl;
                    }
                    throw __FUNCTION__;
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
                        DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                    catch (...) {
                        std::cerr << __FUNCTION__ << " failed" << std::endl;
                    }
                    throw __FUNCTION__;
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
                        throw __FUNCTION__;
                    }
                    catch (JSONScalarConversionError const &e) {
                        DT_LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError" << std::endl;
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
                        throw __FUNCTION__;
                    }
                    catch (JSONScalarConversionError const &e) {
                        DT_LOG(9) << __FUNCTION__ << " successful, got JSONScalarConversionError" << std::endl;
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
                        assert(obj.ceRequired == true);
                        DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                    catch (...) {
                        std::cerr << __FUNCTION__ << " failed" << std::endl;
                    }
                    throw __FUNCTION__;
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
                        DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                    catch (...) {
                        std::cerr << __FUNCTION__ << " failed" << std::endl;
                    }
                    throw __FUNCTION__;
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
                        assert(obj.projectId && obj.projectId.value() == "phs-1234");
                        DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                    catch (...) {
                        std::cerr << __FUNCTION__ << " failed" << std::endl;
                    }
                    throw __FUNCTION__;
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
                        assert(obj.expirationDate && obj.expirationDate.value() == "2012-01-19T20:14:00Z");
                        DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                    catch (...) {
                        std::cerr << __FUNCTION__ << " failed" << std::endl;
                    }
                    throw __FUNCTION__;
                }
                void defaultedRegionTest() {
                    try {
                        auto const obj = makeFrom(R"###(
                            {
                                "service": "ncbi",
                                "link": "https://foo.bar.baz"
                            }
                            )###");
                        assert(obj.region == "be-md");
                        DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                    catch (...) {
                        std::cerr << __FUNCTION__ << " failed" << std::endl;
                    }
                    throw __FUNCTION__;
                }
                void missingRegionTest() {
                    try {
                        makeFrom(R"###(
                            {
                                "service": "sra-ncbi",
                                "link": "https://foo.bar.baz"
                            }
                            )###");
                        throw __FUNCTION__;
                    }
                    catch (Response2::DecodingError const &e) {
                        DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                    }
                }
                void missingLinkTest() {
                    try {
                        makeFrom(
                            R"###( { "service": "sra-ncbi" } )###"
                        );
                        throw __FUNCTION__;
                    }
                    catch (Response2::DecodingError const &e) {
                        DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                    }
                }
                void missingServiceTest() {
                    try {
                        makeFrom(R"###( { "link": "https" } )###");
                        throw __FUNCTION__;
                    }
                    catch (Response2::DecodingError const &e) {
                        DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                    }
                }
                void emptyObjectTest() {
                    try {
                        makeFrom("{}");
                        throw __FUNCTION__;
                    }
                    catch (Response2::DecodingError const &e) {
                        DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
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
                    assert(obj.format && obj.format.value() == "text/fasta");
                    DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                catch (...) {
                    std::cerr << __FUNCTION__ << " failed" << std::endl;
                }
                throw __FUNCTION__;
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
                    assert(obj.noqual == true);
                    DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                catch (...) {
                    std::cerr << __FUNCTION__ << " failed" << std::endl;
                }
                throw __FUNCTION__;
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
                    assert(obj.modificationDate && obj.modificationDate.value() == "2016-11-20T07:38:19Z");
                    DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                catch (...) {
                    std::cerr << __FUNCTION__ << " failed" << std::endl;
                }
                throw __FUNCTION__;
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
                    assert(obj.md5 && obj.md5.value() == "323d0b76f741ae0b71aeec0176645301");
                    DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                catch (...) {
                    std::cerr << __FUNCTION__ << " failed" << std::endl;
                }
                throw __FUNCTION__;
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
                    assert(obj.size && obj.size.value() == "842809");
                    DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                catch (...) {
                    std::cerr << __FUNCTION__ << " failed" << std::endl;
                }
                throw __FUNCTION__;
            }
            void defaultedTypeTest() {
                try {
                    auto const obj = makeFrom(R"###(
                        {
                            "object": "wgs|AAAA00",
                            "name": "AAAA02.11"
                        }
                        )###");
                    assert(obj.type == "sra");
                    DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                catch (...) {
                    std::cerr << __FUNCTION__ << " failed" << std::endl;
                }
                throw __FUNCTION__;
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
                    assert(obj.locations.empty());
                    DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
                catch (...) {
                    std::cerr << __FUNCTION__ << " failed" << std::endl;
                }
                throw __FUNCTION__;
            }
            void noDefaultTypeTest() {
                try {
                    makeFrom(R"###( {
                        "name": "SRR000002",
                        "object": "srapub|SRR000002"
                    } )###");
                    throw __FUNCTION__;
                }
                catch (Response2::DecodingError const &e) {
                    DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                }
            }
            void missingTypeTest() {
                try {
                    makeFrom(R"###( { "name": "SRR000002" } )###");
                    throw __FUNCTION__;
                }
                catch (Response2::DecodingError const &e) {
                    DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                }
            }
            void missingNameTest() {
                try {
                    makeFrom(R"###( { "type": "sra" } )###");
                    throw __FUNCTION__;
                }
                catch (Response2::DecodingError const &e) {
                    DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
                }
            }
            void emptyObjectTest() {
                try {
                    makeFrom("{}");
                    throw __FUNCTION__;
                }
                catch (Response2::DecodingError const &e) {
                    DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
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
                assert(obj.query == "SRR867664");
                assert(obj.status == "404");
                assert(obj.message == "No data at given location.region");
                DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
            catch (...) {
                std::cerr << __FUNCTION__ << " failed" << std::endl;
            }
            throw __FUNCTION__;
        }
        void missingMessageTest() {
            try {
                makeFrom(R"###(
                    {
                        "status": 200,
                        "bundle": "foo"
                    }
                )###");
                throw __FUNCTION__;
            }
            catch (Response2::DecodingError const &e) {
                DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
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
                throw __FUNCTION__;
            }
            catch (Response2::DecodingError const &e) {
                DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
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
                throw __FUNCTION__;
            }
            catch (Response2::DecodingError const &e) {
                DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
            }
        }
        void emptyObjectTest() {
            try {
                makeFrom("{}");
                throw __FUNCTION__;
            }
            catch (Response2::DecodingError const &e) {
                DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
            }
        }
    } child;
    Response2Tests() {
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

            DT_LOG(8) << "All SDL response parsing tests passed." << std::endl;
            return;
        }
        catch (char const *function) {
            std::cerr << function << " failed." << std::endl;
        }
        abort();
    }
    static void testEmpty2() {
        try {
            Response2::makeFrom("{}");
            throw __FUNCTION__;
        }
        catch (Response2::DecodingError const &e) {
            DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
        }
    }
    static void testBadVersion() {
        // version is supposed to be a string value.
        try {
            Response2::makeFrom(R"###({ "version": 99 })###");
            throw __FUNCTION__;
        }
        catch (JSONScalarConversionError const &e) {
            DT_LOG(9) << __FUNCTION__ << " successful, got: JSONScalarConversionError" << std::endl;
        }
    }
    static void testBadVersion2() {
        // version is supposed to "2" or "2.x".
        try {
            Response2::makeFrom(R"###({ "version": "99" })###");
            throw __FUNCTION__;
        }
        catch (Response2::DecodingError const &e) {
            assert(e.object == Response2::DecodingError::topLevel);
            assert(e.haveCause("version"));
            DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
        }
    }
    static void testNoResult() {
        try {
            Response2::makeFrom(R"###({ "version": "2" })###");
            DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
        catch (...) {
            std::cerr << __FUNCTION__ << " failed" << std::endl;
        }
        throw __FUNCTION__;
    }
    static void testBadResult() {
        try {
            Response2::makeFrom(
                R"###({
                    "version": "2",
                    "result": [{}]
                })###");
            throw __FUNCTION__;
        }
        catch (Response2::DecodingError const &e) {
            assert(e.object == Response2::DecodingError::result);
            DT_LOG(9) << __FUNCTION__ << " successful, got: " << e << std::endl;
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
            assert(response.status == "500" && response.message == "Internal Server Error");
            DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
        catch (...) {
            std::cerr << __FUNCTION__ << " failed" << std::endl;
        }
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
            DT_LOG(9) << __FUNCTION__ << " successful" << std::endl;
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
        catch (...) {
            std::cerr << __FUNCTION__ << " failed" << std::endl;
        }
    }
};

void Response2::test()  {
    test_vdbcache();
}

void Response2::test_vdbcache() {
    Response2Tests response2Tests;

    auto const testJSON = R"###(
{
    "version": "unstable",
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
                        }
                    ]
                }
            ]
        }
    ]
}
)###";
    auto const &raw = Response2::makeFrom(testJSON);

    assert(raw.results.size() == 1);
    auto passed = false;
    auto const files = raw.results[0].files.size();
    auto sras = 0;
    auto srrs = 0;

    for (auto const &fl : raw.results[0].getByType("sra")) {
        auto const &file = fl.first;

        assert(file.type == "sra");
        ++sras;

        if (file.hasExtension(".pileup")) continue;
        ++srrs;

        auto const vcache = raw.results[0].getCacheFor(fl);
        assert(vcache >= 0);

        auto const &vdbcache = raw.results[0].flat(vcache).second;
        assert(vdbcache.service == "sra-ncbi");
        assert(vdbcache.region == "public");

        passed = true;
    }
    assert(files == 4);
    assert(sras == 2);
    assert(srrs == 1);
    assert(passed);

    DT_LOG(8) << "vdbcache location matching passed." << std::endl;
}
#endif

int main ( int argc, char *argv [] )
{
    try {
        Response2::test();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
    return 0;
}
