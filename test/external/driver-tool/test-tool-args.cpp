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
 *  Tests for arguments parsing
*
*/

#include <ktst/unit_test.hpp>

#include "tool-args.hpp"
#include "command-line.hpp"

using namespace std;

TEST_SUITE(ToolArgsSuite);

TEST_CASE( ParseArgs )
{
    char argv0[10] = "vdb-dump";
    char argv1[10] = "SRR000123";
    char argv2[20] = "--colname_off";
    char argv3[10] = "-l";
    char argv4[10] = "12";
    char argv5[10] = "-h";
    char * argv[] = { argv0, argv1, argv2, argv3, argv4, argv5, nullptr };
    char * envp[] = { nullptr };
    CommandLine cl(6, argv, envp, nullptr);

    Arguments args = argumentsParsed(cl);
    REQUIRE_EQ(1u, args.countOfCommandArguments()); // SRR000123
    auto it = args.begin();
    {
        REQUIRE( (*it).isArgument() );
        REQUIRE_EQ( string("SRR000123"), string((*it).argument) );
        REQUIRE_EQ( 1, (*it).argind );
    }

    ++it;
    REQUIRE( args.end() != it );
    {
        REQUIRE( ! (*it).isArgument() );
        const ParameterDefinition* def = (*it).def;
        REQUIRE_NOT_NULL( def );
        REQUIRE_EQ( string("colname_off"), string(def->name) );
        REQUIRE_EQ( string("N"), string(def->aliases) );
        // REQUIRE_EQ( 32lu, def->bitMask ); // unused for now
        REQUIRE( ! def->hasArgument );
        REQUIRE( ! def->argumentIsOptional ); // set to false if hasArgument is false
        REQUIRE_EQ( 2, (*it).argind );
    }

    ++it;
    REQUIRE( args.end() != it );
    {
        REQUIRE( ! (*it).isArgument() );
        const ParameterDefinition* def = (*it).def;
        REQUIRE_NOT_NULL( def );
        REQUIRE_EQ( string("line_feed"), string(def->name) );
        REQUIRE_EQ( string("l"), string(def->aliases) );
        REQUIRE( def->hasArgument );
        REQUIRE( ! def->argumentIsOptional );

        REQUIRE_EQ( 3, (*it).argind );
        REQUIRE_EQ( string("12"), string((*it).argument) );
    }

    ++it;
    REQUIRE( args.end() != it );
    {
        REQUIRE( ! (*it).isArgument() );
        const ParameterDefinition* def = (*it).def;
        REQUIRE_NOT_NULL( def );
        REQUIRE_EQ( string("help"), string(def->name) );
        REQUIRE_EQ( string("h?"), string(def->aliases) );
        REQUIRE_EQ( 5, (*it).argind );
    }

    ++it;
    REQUIRE( args.end() == it );

    REQUIRE_EQ(3u, args.countOfParameters()); // --colname_off, -l 12, -h
    // REQUIRE_EQ(32lu, args.argsUsed()); // unused

    //TODO: (*it).reason
}

//TODO: more tests

int main ( int argc, char *argv [] )
{
    try {
        return ToolArgsSuite(argc,argv);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
}

