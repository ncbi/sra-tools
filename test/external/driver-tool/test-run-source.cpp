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
 *  Tests for class data_sources
*
*/

#include <ktst/unit_test.hpp>

#include <kfg/config.h>
#include <klib/text.h>

#include "run-source.hpp"
#include "tool-arguments.h"
#include "config.hpp"
#include "service.hpp"
#include "debug.hpp"
#include "build-version.hpp"

using namespace std;

namespace sratools {

    string const *location = NULL;
    FilePath const *perm = NULL;
    FilePath const *ngc = NULL;

    Config const *config = new Config();

    char const *Accession::extensions[] = { ".sra", ".sralite", ".realign", ".noqual", ".sra" };
    char const *Accession::qualityTypes[] = { "Full", "Lite" };
    char const *Accession::qualityTypeForFull = qualityTypes[0];
    char const *Accession::qualityTypeForLite = qualityTypes[1];

    Accession::Accession(string const &value)
    {
    }

    vector<pair<unsigned, unsigned>> Accession::sraExtensions() const
    {
        return vector<pair<unsigned, unsigned>>();
    }

    enum AccessionType Accession::type() const
    {
        return unknown;
    }

}

namespace vdb {
    string Service::CE_Token()
    {
        return std::string();
    }
    void Service::add(vector<string> const &terms) const
    {
    }
    void Service::setLocation(string const &location) const
    {
    }
    void Service::setPermissionsFile(std::string const &path) const
    {
    }
    Service::QualityType prefQualityType = vdb::Service::unknown;
    Service::QualityType Service::preferredQualityType()
    {
        return unknown;
    }
    void Service::setNGCFile(std::string const &path) const
    {
    }
    string ServiceResponse;
    string URL;
    Service::Response Service::response(std::string const &url, std::string const &version) const
    {
        URL = url;
        return Response(nullptr, ServiceResponse.c_str());
    }
    Service::Response::~Response()
    {
    }
    std::ostream &operator <<(std::ostream &os, Service::Response const &rhs)
    {
        return os;
    }
    Service Service::make()
    {
        return Service(nullptr);
    }
    Service::Service(void *obj){}
    Service::~Service(){}
}

////////////////////
// stub for Config, use test flags instead of KConfig
static bool sdlDisabled = true;
sratools::Config::Config()
{
    obj = NULL;
    rc_t rc = KConfigMake((KConfig **)&obj, NULL);
    if (rc != 0) {
        assert(obj != NULL);
        throw std::runtime_error("failed to load configuration");
    }
}
sratools::Config::~Config()
{
    KConfigRelease((KConfig *)obj);
}
opt_string sratools::Config::get(char const *const keypath) const
{
    if ( string(keypath) == "/repository/remote/disabled" )
    {
        return sdlDisabled ? opt_string("true") : opt_string();
    }

    String *value = NULL;
    KConfigReadString((KConfig *)obj, keypath, &value);
    opt_string result;
    if (value) {
        result = std::string(value->addr, value->size);
        free(value);
    }
    //cout<<"key="<<keypath << " result='" << (result?*result:"none") << "'" << endl;
    return result;
}

TEST_SUITE(DataSourcesSuite);

string ExePath;
string ExeName;

TEST_CASE(CopmmandLine_GetToolPath)
{
    const char* argv[] = { TOOL_NAME_SRA_PILEUP };
    char* env[] = { (char*)"a=b", nullptr };
    char* extra[] = { (char*)"a1=b1", nullptr };
    CommandLine cl(1, (char**)argv, env, extra);
    REQUIRE_NE( string::npos, ((string)cl.toolPath).find( TOOL_NAME_SRA_PILEUP ) );
    //cout << "toolPath=" << (string)cl.toolPath << endl;
}

TEST_CASE( CommandLine_Defaults )
{
    char argv0[10] = "vdb-dump";
    char * argv[] = { argv0, nullptr };
    char * envp[] = { nullptr };
    CommandLine cl(1, argv, envp, nullptr);
    data_sources ds = data_sources::preload(cl, argumentsParsed(cl));

    REQUIRE_EQ( (const char* const*)&argv, cl.argv );
    REQUIRE_EQ( (const char* const*)&envp, cl.envp );
    REQUIRE_NULL( cl.extra );

//TODO
// #if USE_WIDE_API
//     wchar_t const* const* wargv;
//     wchar_t const* const* wenvp;
// #endif

#if WINDOWS
#else
    REQUIRE_NULL( cl.fakeName );
#endif

    REQUIRE_NOT_NULL( cl.baseName );
    REQUIRE_EQ( string(argv0), string(cl.baseName) );
    REQUIRE_EQ( string(argv0), cl.toolName );
//    REQUIRE_EQ( ExePath, ((string)cl.fullPathToExe) );
//    REQUIRE_EQ( string(), ((string)cl.fullPath) ); // .../test-bin
    auto const expectToolPath = ((string)cl.fullPath)+"/"+argv0;
    auto const actualToolPath = (string)cl.toolPath;
    REQUIRE_GE( actualToolPath.size(), expectToolPath.size() );
    if (actualToolPath.size() == expectToolPath.size()) {
        REQUIRE_EQ( expectToolPath, actualToolPath );
    }
    else {
        REQUIRE_EQ( expectToolPath, actualToolPath.substr(0, expectToolPath.size()) );
    }
//    REQUIRE_EQ( ExeName, cl.realName );
    REQUIRE_EQ( 1, cl.argc );
    REQUIRE_EQ( sratools::Version::current.packed, cl.buildVersion );

// #if WINDOWS
// #else
//     uint32_t versionFromName, runAsVersion;
// #endif
//TODO:
//     FilePath pathForArgument(int index, int offset = 0) const
//     FilePath pathForArgument(Argument const &arg) const
//     char const *runAs() const
//     bool isShortCircuit() const
}

TEST_CASE( UnknownTool )
{
    char argv0[20] = { "not-a-tool-at-all" };
    char * argv[] = { argv0 };
    char * envp[] = { nullptr };
    CommandLine cl(1, argv, envp, nullptr);
    try
    {
        data_sources ds = data_sources::preload(cl, argumentsParsed(cl));
        FAIL("Expected exception not thrown");
    }
    catch(const UnknownToolException&)
    {
    }
}

TEST_CASE( URL_parameter )
{
    char argv0[] = "vdb-dump";
    char argv1[] = "file:///c:/autoexec.bat";
    char *argv[] = { argv0, argv1, nullptr };
    char *envp[] = { nullptr };
    auto const &cl = CommandLine(2, argv, envp, nullptr);
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);
    auto const &acc = ds[argv1];
    auto iter = acc.begin();
    REQUIRE(iter != acc.end());

    // The expectation is that the argument will simply be
    // passed along with nothing added to the environment.
    REQUIRE((*iter).environment.size() == 0);
}

class DataSourcesFixture
{
protected:
    static char argv0[];
    static char argv1[];
    static char * argv[];
    static char * envp[];

    DataSourcesFixture()
    : cl(2, argv, envp, nullptr)
    {}

    CommandLine cl;
};

char DataSourcesFixture::argv0[] = "vdb-dump";
char DataSourcesFixture::argv1[] = "SRR000123";
char * DataSourcesFixture::argv[] = { argv0, argv1, nullptr };
char * DataSourcesFixture::envp[] = { nullptr };

FIXTURE_TEST_CASE( ConstructNoSDL, DataSourcesFixture )
{
// the two arg constructor for data_sources is only use if SRATOOLS_TESTING=2
    auto const &ds = data_sources(cl, argumentsParsed(cl));
    REQUIRE( ds.ce_token().empty() );
    {
        auto acc = ds[argv1];
        auto it = acc.begin();
        REQUIRE( acc.end() != it );

        REQUIRE_EQ( string(argv1), (*it).service );
        REQUIRE( ! (*it).qualityType.has_value() );
        REQUIRE( ! (*it).project.has_value() );

        {
            const auto & env = (*it).environment;
            auto env_it = env.begin();
            REQUIRE_EQ( string("VDB_LOCAL_URL"), string( (*env_it).first) );
            REQUIRE_EQ( string(argv1), string( (*env_it).second) );
            ++env_it;
            REQUIRE( env_it == env.end() );
        }

        ++it;
        REQUIRE( acc.end() == it );
    }
    auto qi = ds.queryInfo;
    REQUIRE_EQ( (size_t)1, qi.size() );
    auto dict = qi[argv1];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string(argv1), string(dict["local"]) );
    REQUIRE_EQ( string(argv1), string(dict["local/filePath"]) );
    REQUIRE_EQ( string(argv1), string(dict["name"]) );
}

static bool is_unknown_from_file_system(data_sources::accession const &acc)
{
    auto it = acc.begin();
    return  it != acc.end() &&
            (*it).service == "the file system" &&
            !(*it).qualityType.has_value() &&
            !(*it).project.has_value() &&
            (++it) == acc.end();
}

FIXTURE_TEST_CASE( ConstructSDL_false, DataSourcesFixture )
{
    auto const &ds = data_sources(cl, argumentsParsed(cl), false);

    REQUIRE( ds.ce_token().empty() );
    REQUIRE(is_unknown_from_file_system(ds[argv1]));

    auto qi = ds.queryInfo;
    REQUIRE_EQ( (size_t)1, qi.size() );  // cf. ConstructNoSDL
    auto dict = qi[argv1];
    REQUIRE(DictionaryValueMatches(dict, "name", argv1));
    REQUIRE(DictionaryHasKey(dict, "simple"));
}

FIXTURE_TEST_CASE( ConstructSDL_true, DataSourcesFixture )
{
    /*
    const opt_string url = sratools::config->get("/repository/remote/main/SDL.2/resolver-cgi");
    REQUIRE_EQ( *url, vdb::URL );
    */
    vdb::ServiceResponse = "{}";

    auto const &ds = data_sources(cl, argumentsParsed(cl), true);

    REQUIRE( ds.ce_token().empty() );
    REQUIRE(is_unknown_from_file_system(ds[argv1]));

    auto qi = ds.queryInfo;
    REQUIRE_EQ( (size_t)1, qi.size() );
    auto dict = qi[argv1];
    REQUIRE(DictionaryValueMatches(dict, "name", argv1));
    REQUIRE(DictionaryHasKey(dict, "simple"));
}

FIXTURE_TEST_CASE( ConstructSDL_badSDL, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","resu)";
    REQUIRE_THROW( data_sources(cl, argumentsParsed(cl), true) );
}

FIXTURE_TEST_CASE( ConstructSDL_noResults, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[]})";
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);
    REQUIRE( ds.ce_token().empty() );
    REQUIRE(is_unknown_from_file_system(ds[argv1]));

    auto qi = ds.queryInfo;
    REQUIRE_EQ( (size_t)1, qi.size() );
    auto dict = qi[argv1];
    REQUIRE(DictionaryValueMatches(dict, "name", argv1));
    REQUIRE(DictionaryHasKey(dict, "simple"));
}

FIXTURE_TEST_CASE( ConstructSDL_oneResult, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR000001",
                    "accession": "SRR000001",
                    "type": "sra",
                    "name": "SRR000001",
                    "size": 312527083,
                    "md5": "9bde35fefa9d955f457e22d9be52bcd9",
                    "modificationDate": "2015-04-08T02:54:13Z",
                    "locations": [
                        {
                            "service": "s3",
                            "region": "us-east-1",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"
                        }
                    ]
                }
            ]
        }]})";
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);

    REQUIRE(is_unknown_from_file_system(ds[argv1]));

    auto qi = ds.queryInfo;
    REQUIRE_EQ( (size_t)2, qi.size() );
    {
        auto dict = qi[argv1];
        REQUIRE(DictionaryValueMatches(dict, "name", argv1));
        REQUIRE(DictionaryHasKey(dict, "simple"));
    }
    {
        auto dict = qi["SRR000001"];
        REQUIRE_EQ( (size_t)9, dict.size() );
        REQUIRE_EQ( string("1"), string(dict["remote"]) ); // # of files added
        REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
        REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
        REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
        REQUIRE_EQ( string("https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"),
                    string(dict["remote/1/filePath"]) );
        REQUIRE_EQ( string("312527083"), string(dict["remote/1/fileSize"]) );
        REQUIRE_EQ( string("Full"), string(dict["remote/1/qualityType"]) );
        REQUIRE_EQ( string("us-east-1"), string(dict["remote/1/region"]) );
        REQUIRE_EQ( string("s3"), string(dict["remote/1/service"]) );
    }
}

FIXTURE_TEST_CASE( ConstructSDL_multipleFiles, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR000001",
                    "accession": "SRR000001",
                    "type": "sra",
                    "name": "SRR000001",
                    "size": 312527083,
                    "md5": "9bde35fefa9d955f457e22d9be52bcd9",
                    "modificationDate": "2015-04-08T02:54:13Z",
                    "locations": [
                        {
                            "service": "s3",
                            "region": "us-east-1",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"
                        }
                    ]
                },
                {
                    "object": "sraxyz|SRR000001",
                    "accession": "SRR000002",
                    "type": "sra",
                    "name": "SRR000002",
                    "size": 312527084,
                    "md5": "9bde35fefa9d955f457e22d9be52bcda",
                    "modificationDate": "2015-04-08T02:54:14Z",
                    "locations": [
                        {
                            "service": "s4",
                            "region": "us-east-2",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000002/SRR000002"
                        }
                    ]
                }
            ]
        }]})";
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);

    REQUIRE(is_unknown_from_file_system(ds[argv1]));

    auto qi = ds.queryInfo;
    REQUIRE_EQ( (size_t)2, qi.size() );
    REQUIRE(DictionaryValueMatches(qi[argv1], "name", argv1));
    {
        auto dict = qi["SRR000001"];
        // for(auto i : dict)
        //     cout << i.first << ":'" << i.second << "'" << endl;

        REQUIRE_EQ( (size_t)14, dict.size() );

        REQUIRE_EQ( string("2"), string(dict["remote"]) ); // # of files added

        REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
        REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
        REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );

        // the N in the key remote/N/... is assigned based on the sort order of the value of "object"
        // the value of "object" itself is not saved
        REQUIRE_EQ( string("https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"),
                    string(dict["remote/1/filePath"]) );
        REQUIRE_EQ( string("312527083"),    string(dict["remote/1/fileSize"]) );
        REQUIRE_EQ( string("Full"),         string(dict["remote/1/qualityType"]) );
        REQUIRE_EQ( string("us-east-1"),    string(dict["remote/1/region"]) );
        REQUIRE_EQ( string("s3"),           string(dict["remote/1/service"]) );

        REQUIRE_EQ( string("https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000002/SRR000002"),
                    string(dict["remote/2/filePath"]) );
        REQUIRE_EQ( string("312527084"),    string(dict["remote/2/fileSize"]) );
        REQUIRE_EQ( string("Full"),         string(dict["remote/2/qualityType"]) );
        REQUIRE_EQ( string("us-east-2"),    string(dict["remote/2/region"]) );
        REQUIRE_EQ( string("s4"),           string(dict["remote/2/service"]) );
    }
}

FIXTURE_TEST_CASE( ConstructSDL_multipleLocations, DataSourcesFixture )
{   // only the first suitable location is used
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR000001",
                    "accession": "SRR000001",
                    "type": "sra",
                    "name": "SRR000001",
                    "size": 312527083,
                    "md5": "9bde35fefa9d955f457e22d9be52bcd9",
                    "modificationDate": "2015-04-08T02:54:13Z",
                    "locations": [
                        {
                            "service": "s3",
                            "region": "us-east-1",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"
                        },
                        {
                            "service": "s4",
                            "region": "us-west-1",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000002/SRR000002"
                        }
                    ]
                }
            ]
        }]})";
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);

    REQUIRE(is_unknown_from_file_system(ds[argv1]));

    auto qi = ds.queryInfo;
    REQUIRE_EQ( (size_t)2, qi.size() );
    REQUIRE(DictionaryValueMatches(qi[argv1], "name", argv1));
    {
        auto dict = qi["SRR000001"];
        REQUIRE(DictionaryValueMatches(dict, "remote", "2"));
        REQUIRE(DictionaryValueMatches(dict, "remote/1/filePath", "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"));
        REQUIRE(DictionaryValueMatches(dict, "remote/2/filePath", "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000002/SRR000002"));
    }
}

FIXTURE_TEST_CASE( ConstructSDL_status_400, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 400,
            "msg": "no data"
        }]})";
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);
    auto qi = ds.queryInfo;
    auto dict = qi[ "SRR000001" ];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("400"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("no data"), string(dict["SDL/message"]) );
}

FIXTURE_TEST_CASE( ConstructSDL_status_404, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 404,
            "msg": "no data"
        }]})";
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);
    auto qi = ds.queryInfo;
    auto dict = qi["SRR000001"];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("404"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("no data"), string(dict["SDL/message"]) );
}

FIXTURE_TEST_CASE( ConstructSDL_pileupIgnored, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR000001",
                    "accession": "SRR000001",
                    "type": "sra",
                    "name": "SRR000001.pileup",
                    "size": 312527083,
                    "md5": "9bde35fefa9d955f457e22d9be52bcd9",
                    "modificationDate": "2015-04-08T02:54:13Z",
                    "locations": [
                        {
                            "service": "s3",
                            "region": "us-east-1",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"
                        }
                    ]
                }
            ]
        }]})";
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);
    auto qi = ds.queryInfo;
    auto dict = qi["SRR000001"];

    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    // no other info; the SRR000001.pileup is ignored
}

FIXTURE_TEST_CASE( ConstructSDL_noObject_Ignored, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "accession": "SRR000001",
                    "type": "sra",
                    "name": "SRR000001",
                    "size": 312527083,
                    "md5": "9bde35fefa9d955f457e22d9be52bcd9",
                    "modificationDate": "2015-04-08T02:54:13Z",
                    "locations": [
                        {
                            "service": "s3",
                            "region": "us-east-1",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"
                        }
                    ]
                }
            ]
        }]})";
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);
    auto qi = ds.queryInfo;
    auto dict = qi["SRR000001"];

    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    // no other info; SRR000001 is ignored since there is no 'object' field
}

FIXTURE_TEST_CASE( ConstructSDL_noQuality_Ignored, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR000001",
                    "accession": "SRR000001",
                    "type": "noqual_sra",
                    "name": "SRR000001",
                    "size": 312527083,
                    "md5": "9bde35fefa9d955f457e22d9be52bcd9",
                    "modificationDate": "2015-04-08T02:54:13Z",
                    "locations": [
                        {
                            "service": "s3",
                            "region": "us-east-1",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"
                        }
                    ]
                }
            ]
        }]})";
    vdb::prefQualityType = vdb::Service::full;
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);
    auto qi = ds.queryInfo;
    auto dict = qi["SRR000001"];

    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    // no other info; SRR000001 is ignored b/c of "type": "noqual_sra" and config preference for full qualities
}

FIXTURE_TEST_CASE( ConstructSDL_Encrypted_NoNGC_Skipped, DataSourcesFixture )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR000001",
                    "accession": "SRR000001",
                    "type": "noqual_sra",
                    "name": "SRR000001",
                    "size": 312527083,
                    "encryptedForProjectId": 12345,
                    "md5": "9bde35fefa9d955f457e22d9be52bcd9",
                    "modificationDate": "2015-04-08T02:54:13Z",
                    "locations": [
                        {
                            "service": "s3",
                            "region": "us-east-1",
                            "link": "https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"
                        }
                    ]
                }
            ]
        }]})";
    vdb::prefQualityType = vdb::Service::full;
    auto const &ds = data_sources(cl, argumentsParsed(cl), true);
    auto qi = ds.queryInfo;
    auto dict = qi["SRR000001"];

    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    // no other info; SRR000001 is ignored b/c encrypted and no NGC
}

int main ( int argc, char *argv [] )
{
    ExePath = argv[0];
    try {
        int ret = DataSourcesSuite(argc,argv);
        delete sratools::config;
        return ret;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
}

