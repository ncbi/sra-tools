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
    char * argv[] = { argv0 };
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
    REQUIRE_EQ( ExePath, ((string)cl.fullPathToExe) );
//    REQUIRE_EQ( string(), ((string)cl.fullPath) ); // .../test-bin
    REQUIRE_EQ( ((string)cl.fullPath)+"/"+argv0, ((string)cl.toolPath) );
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

char argv0[10] = "vdb-dump";
char argv1[10] = "SRR000123";
char * argv[] = { argv0, argv1, nullptr };
char * envp[] = { nullptr };

TEST_CASE( ConstructNoSDL )
{
    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl)); // the non-SDL ctor

    REQUIRE( ds.ce_token().empty() );

    data_sources::accession::const_iterator it = ds[argv1].begin();
    REQUIRE( ds[argv1].end() != it );

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
    REQUIRE( ds[argv1].end() == it );

    REQUIRE_EQ( (size_t)1, ds.queryInfo.size() );
    auto dict = ds.queryInfo[ argv1 ];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string(argv1), string(dict["local"]) );
    REQUIRE_EQ( string(argv1), string(dict["local/filePath"]) );
    REQUIRE_EQ( string(argv1), string(dict["name"]) );
}

TEST_CASE( ConstructSDL_false )
{
    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl),false); // the SDL ctor, no SDL use

    REQUIRE( ds.ce_token().empty() );

    data_sources::accession::const_iterator it = ds[argv1].begin();
    REQUIRE( ds[argv1].end() != it );

    REQUIRE_EQ( string("the file system"), (*it).service ); // cf. ConstructNoSDL
    REQUIRE( ! (*it).qualityType.has_value() );
    REQUIRE( ! (*it).project.has_value() );

    REQUIRE_EQ( (size_t)0, (*it).environment.size() );  // cf. ConstructNoSDL

    ++it;
    REQUIRE( ds[argv1].end() == it );

    REQUIRE_EQ( (size_t)1, ds.queryInfo.size() );  // cf. ConstructNoSDL
    auto dict = ds.queryInfo[ argv1 ];
    REQUIRE_EQ( (size_t)1, dict.size() );
    REQUIRE_EQ( string(argv1), string(dict["name"]) );
}

TEST_CASE( ConstructSDL_true )
{
    const opt_string url = sratools::config->get("/repository/remote/main/SDL.2/resolver-cgi");
    vdb::ServiceResponse = "{}";

    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl), true); // the SDL ctor, use SDL
    REQUIRE_EQ( *url, vdb::URL );

    REQUIRE( ds.ce_token().empty() );

    data_sources::accession::const_iterator it = ds[argv1].begin();
    REQUIRE( ds[argv1].end() != it );

    REQUIRE_EQ( string("the file system"), (*it).service );
    REQUIRE( ! (*it).qualityType.has_value() );
    REQUIRE( ! (*it).project.has_value() );

    REQUIRE_EQ( (size_t)0, (*it).environment.size() );

    ++it;
    REQUIRE( ds[argv1].end() == it );

    REQUIRE_EQ( (size_t)1, ds.queryInfo.size() );
    auto dict = ds.queryInfo[ argv1 ];
    REQUIRE_EQ( (size_t)1, dict.size() );
    REQUIRE_EQ( string(argv1), string(dict["name"]) );
}

TEST_CASE( ConstructSDL_badSDL )
{
    vdb::ServiceResponse = R"({"version": "2","resu)";
    CommandLine cl(2, argv, envp, nullptr);
    REQUIRE_THROW( data_sources(cl, argumentsParsed(cl), true) );
}

TEST_CASE( ConstructSDL_noResults )
{
    vdb::ServiceResponse = R"({"version": "2","result":[]})";

    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl), true);

    REQUIRE( ds.ce_token().empty() );

    data_sources::accession::const_iterator it = ds[argv1].begin();
    REQUIRE( ds[argv1].end() != it );

    REQUIRE_EQ( string("the file system"), (*it).service );
    REQUIRE( ! (*it).qualityType.has_value() );
    REQUIRE( ! (*it).project.has_value() );

    REQUIRE_EQ( (size_t)0, (*it).environment.size() );

    ++it;
    REQUIRE( ds[argv1].end() == it );

    REQUIRE_EQ( (size_t)1, ds.queryInfo.size() );
    auto dict = ds.queryInfo[ argv1 ];
    REQUIRE_EQ( (size_t)1, dict.size() );
    REQUIRE_EQ( string(argv1), string(dict["name"]) );
}

TEST_CASE( ConstructSDL_oneResult )
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

    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl), true);

    REQUIRE( ds.ce_token().empty() );

    data_sources::accession::const_iterator it = ds[argv1].begin();
    REQUIRE( ds[argv1].end() != it );

    REQUIRE_EQ( string("the file system"), (*it).service );
    REQUIRE( ! (*it).qualityType.has_value() );
    REQUIRE( ! (*it).project.has_value() );

    REQUIRE_EQ( (size_t)0, (*it).environment.size() );

    ++it;
    REQUIRE( ds[argv1].end() == it );

    REQUIRE_EQ( (size_t)2, ds.queryInfo.size() );
    auto dict = ds.queryInfo[ argv1 ];
    REQUIRE_EQ( (size_t)1, dict.size() );
    REQUIRE_EQ( string(argv1), string(dict["name"]) );

    dict = ds.queryInfo[ "SRR000001" ];
    REQUIRE_EQ( (size_t)9, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    REQUIRE_EQ( string("1"), string(dict["remote"]) );
    REQUIRE_EQ( string("https://sra-pub-run-odp.s3.amazonaws.com/sra/SRR000001/SRR000001"),
                string(dict["remote/1/filePath"]) );
    REQUIRE_EQ( string("312527083"), string(dict["remote/1/fileSize"]) );
    REQUIRE_EQ( string("Full"), string(dict["remote/1/qualityType"]) );
    REQUIRE_EQ( string("us-east-1"), string(dict["remote/1/region"]) );
    REQUIRE_EQ( string("s3"), string(dict["remote/1/service"]) );
}

TEST_CASE( ConstructSDL_status_400 )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 400,
            "msg": "no data"
        }]})";

    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl), true);

    auto dict = ds.queryInfo[ "SRR000001" ];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("400"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("no data"), string(dict["SDL/message"]) );
}

TEST_CASE( ConstructSDL_status_404 )
{
    vdb::ServiceResponse = R"({"version": "2","result":[{
            "bundle": "SRR000001",
            "status": 404,
            "msg": "no data"
        }]})";

    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl), true);

    auto dict = ds.queryInfo[ "SRR000001" ];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("404"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("no data"), string(dict["SDL/message"]) );
}

TEST_CASE( ConstructSDL_pileupIgnored )
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

    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl), true);

    auto dict = ds.queryInfo[ "SRR000001" ];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    // no other info; the SRR000001.pileup is ignored
}

TEST_CASE( ConstructSDL_noObject_Ignored )
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

    CommandLine cl(2, argv, envp, nullptr);
    data_sources ds(cl, argumentsParsed(cl), true);

    auto dict = ds.queryInfo[ "SRR000001" ];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    // no other info; SRR000001 is ignored since there is no 'object' field
}

TEST_CASE( ConstructSDL_noQuality_Ignored )
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

    CommandLine cl(2, argv, envp, nullptr);
    vdb::prefQualityType = vdb::Service::full;
    data_sources ds(cl, argumentsParsed(cl), true);

    auto dict = ds.queryInfo[ "SRR000001" ];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    // no other info; SRR000001 is ignored b/c of "type": "noqual_sra" and config preference for full qualities
}

TEST_CASE( ConstructSDL_Encrypted_NoNGC_Skipped )
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

    CommandLine cl(2, argv, envp, nullptr);
    vdb::prefQualityType = vdb::Service::full;
    data_sources ds(cl, argumentsParsed(cl), true);

    auto dict = ds.queryInfo[ "SRR000001" ];
    REQUIRE_EQ( (size_t)3, dict.size() );
    REQUIRE_EQ( string("SRR000001"), string(dict["accession"]) );
    REQUIRE_EQ( string("200"), string(dict["SDL/status"]) );
    REQUIRE_EQ( string("ok"), string(dict["SDL/message"]) );
    // no other info; SRR000001 is ignored b/c encrypted and no NGC
}

#if WIN32
#define main wmain
#endif

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

