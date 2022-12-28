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

TEST_CASE(CopmmandLine_GetToolPath)
{
    const char* argv[] = { TOOL_NAME_SRA_PILEUP };
    char* env[] = { (char*)"a=b", nullptr };
    char* extra[] = { (char*)"a1=b1", nullptr };
    CommandLine cl(1, (char**)argv, env, extra);
    cout << "toolPath=" << (string)cl.toolPath << endl;
}

TEST_CASE( Defaults )
{
    char argv0[10] = "vdb-dump";
    char * argv[] = { argv0 };
    char * envp[] = { nullptr };
    CommandLine cl(1, argv, envp, nullptr);
    data_sources ds = data_sources::preload(cl, argumentsParsed(cl));
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
    CommandLine cl(2, argv, envp, nullptr);

    opt_string url = sratools::config->get("/repository/remote/main/SDL.2/resolver-cgi");
    vdb::ServiceResponse = "{}";
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

#if WIN32
#define main wmain
#endif

int main ( int argc, char *argv [] )
{
    try {
        return DataSourcesSuite(argc,argv);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
}

