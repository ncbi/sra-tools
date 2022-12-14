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
    Service::Response Service::response(std::string const &url, std::string const &version) const
    {
        return Response(nullptr, "");
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

// namespace sratools {
//     Config const *config = new Config();
//     string const *location = nullptr;
//     FilePath const *perm = nullptr;
//     FilePath const *ngc = nullptr;
// }

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
    }
    catch(const UnknownToolException&)
    {
    }
}

TEST_CASE( TestingLevelDefault )
{
    REQUIRE_EQ(0, logging_state::testing_level() ); // ensures that data_sources::preload() checks canUseSDL
}

TEST_CASE( SetNoSDL )
{
    EnvironmentVariables::set("SRATOOLS_TESTING", "2");
    REQUIRE_EQ(opt_string("2"), EnvironmentVariables::get("SRATOOLS_TESTING") );
    REQUIRE_EQ(2, logging_state::testing_level() );
    EnvironmentVariables::set("SRATOOLS_TESTING", "0");
}

TEST_CASE( ParseArgs )
{
    EnvironmentVariables::set("SRATOOLS_TESTING", "2");
    REQUIRE_EQ(opt_string("2"), EnvironmentVariables::get("SRATOOLS_TESTING") );
    REQUIRE_EQ(2, logging_state::testing_level() );

    char argv0[10] = "vdb-dump";
    char argv1[10] = "SRR000123";
    char argv2[20] = "--colname_off";
    char argv3[10] = "--l 12";
    char * argv[] = { argv0, argv1, argv2, argv3 };
    char * envp[] = { nullptr };
    CommandLine cl(4, argv, envp, nullptr);

    Arguments args = argumentsParsed(cl);
    REQUIRE_EQ(1u, args.countOfCommandArguments()); // SRR000123
    auto it = args.begin();
    {
        const ParameterDefinition* def = (*it).def;
        REQUIRE_NOT_NULL( def );
        REQUIRE_NULL( def->name );
        REQUIRE_NULL( def->aliases );
        REQUIRE_EQ( 0lu, def->bitMask );
        REQUIRE( def->hasArgument );
        REQUIRE( ! def->argumentIsOptional );
    }

    ++it;
    REQUIRE( args.end() != it );
    {
        const ParameterDefinition* def = (*it).def;
        REQUIRE_NOT_NULL( def );
        REQUIRE_EQ( string("colname_off"), string(def->name) );
        REQUIRE_EQ( string("N"), string(def->aliases) );
        REQUIRE_EQ( 32lu, def->bitMask ); //?
        REQUIRE( ! def->hasArgument );
        REQUIRE( ! def->argumentIsOptional ); // set to false if hasArgument is false ?
    }

    ++it;
    REQUIRE( args.end() != it );
    {
        const ParameterDefinition* def = (*it).def;
        REQUIRE_NOT_NULL( def );
        REQUIRE_NULL( def->name ); //????
        // REQUIRE_EQ( string("line_feed"), string(def->name) );
        REQUIRE_NULL( def->aliases ); // ?????
        // REQUIRE_EQ( string("l"), string(def->aliases) );
        REQUIRE_EQ( 0lu, def->bitMask ); //?
        REQUIRE( ! def->hasArgument );
        REQUIRE( ! def->argumentIsOptional ); 
    }

    ++it;
    REQUIRE( args.end() == it );

    REQUIRE_EQ(2u, args.countOfParameters()); // --colname_off, --l 12
    REQUIRE_EQ(32lu, args.argsUsed()); // ??????????

}

TEST_CASE( PreloadNoSDL )
{
    EnvironmentVariables::set("SRATOOLS_TESTING", "2");
    REQUIRE_EQ(opt_string("2"), EnvironmentVariables::get("SRATOOLS_TESTING") );
    REQUIRE_EQ(2, logging_state::testing_level() );

    char argv0[10] = "vdb-dump";
    char argv1[10] = "SRR000123";
    char argv2[20] = "--colname_off";
    char argv3[10] = "--l 12";
    char * argv[] = { argv0, argv1, argv2, argv3 };
    char * envp[] = { nullptr };
    CommandLine cl(4, argv, envp, nullptr);

    data_sources ds = data_sources::preload(cl, argumentsParsed(cl));
    data_sources::accession::const_iterator it = ds[argv1].begin();
    REQUIRE( ds[argv1].end() != it );

    REQUIRE_EQ( string(argv1), (*it).service );
    REQUIRE( ! (*it).qualityType.has_value() );
    REQUIRE( ! (*it).project.has_value() );
    for (auto const& y : (*it).environment)
        cout << "  env: " << y.first << ": " << y.second << endl;
    ++it;
    REQUIRE( ds[argv1].end() == it );

    for (auto const& x : ds.queryInfo)
    {
        cout << "  " << x.first << ": " << endl;
        for (auto const& y : x.second)
            cout << "    " << y.first << ": " << y.second << endl;
    }

    EnvironmentVariables::set("SRATOOLS_TESTING", "0");
}

// TEST_CASE(Construct_NoArgs_NoSDL)
// {
//     char* argv[] = { (char*)TOOL_NAME_SRA_PILEUP };
//     char* env[] = { nullptr };
//     unique_ptr<CommandLine> cmdline ( new CommandLine(1, argv, env, nullptr) );

//     data_sources ds = data_sources::preload( *cmdline, argumentsParsed(*cmdline) );

//     REQUIRE( ds.ce_token().empty() );
// }

// TEST_CASE(QueryInfo)
// {
//     const char* argv[] = { TOOL_NAME_SRA_PILEUP, "SRR000123", "-v" };
//     char* env[] = { (char*)"a=b", nullptr };
//     char* extra[] = { (char*)"a1=b1", nullptr };
//     CommandLine cl(3, (char**)argv, env, extra);

//     cout << "argc=" << cl.argc << endl;
//     for (int i = 0; i < cl.argc; ++i)
//         cout << "argv=" << cl.argv[i] << endl;

//     {
//         int i=0;
//         while( cl.envp[i] != nullptr )
//         {
//             cout << "envp["<<i<<"]=" << cl.envp[i] << endl;
//             ++i;
//         }
//     }

//     {
//         int i=0;
//         while( cl.extra[i] != nullptr )
//         {
//             cout << "extra["<<i<<"]=" << cl.extra[i] << endl;
//             ++i;
//         }
//     }

//     cout << "fakeName=" << (cl.fakeName?cl.fakeName:"<null>") << endl;
//     cout << "baseName=" << cl.baseName << endl;
//     cout << "toolName=" << cl.toolName << endl;
//     cout << "fullPathToExe=" << (string)cl.fullPathToExe << endl;
//     cout << "fullPath=" << (string)cl.fullPath << endl;
// //    cout << "toolPath=" << (string)cl.toolPath << endl;
//     // cout << "realName=" << cl.realName << endl;
//     // cout << "buildVersion=" << cl.buildVersion << endl;
//     // cout << "versionFromName=" << cl.versionFromName << endl;
//     // cout << "runAsVersion=" << cl.runAsVersion << endl;

//     data_sources ds = data_sources::preload( cl, argumentsParsed(cl) );

// //    data_sources::accession acc = ds["v"];
// //    REQUIRE_EQ( string("arg1"), ds["name"]);
// }

//TODO: data_sources::preload with testing_level == 2

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

