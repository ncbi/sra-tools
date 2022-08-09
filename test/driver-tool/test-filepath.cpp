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
 *  Tests for POSIX::FilePath and Win32::FilePath
*
*/

#include <ktst/unit_test.hpp>

#include "file-path.hpp"
#include "file-path.posix.cpp"

using namespace std;

TEST_SUITE(FilePathSuite);

TEST_CASE( DefaultConstruct )
{
    FilePath_<PlatformFilePath> fp;
    REQUIRE( fp.empty() );
    REQUIRE_EQ( (size_t)0, fp.size() );
    REQUIRE_EQ( string(), (string)fp );
    REQUIRE( ! fp.implementation().getOwns() );
}

TEST_CASE( Construct_Cstring )
{
    const char * path = "/a/b/c/filename.ext";
    FilePath_<PlatformFilePath> fp( path );
    REQUIRE( ! fp.empty() );
    REQUIRE_EQ( string( path ), (string)fp );
    REQUIRE_EQ( strlen( path ), fp.size() );
    REQUIRE( ! fp.implementation().getOwns() );
}

TEST_CASE( Construct_Cstring_Null )
{
    FilePath_<PlatformFilePath> fp( nullptr );
    REQUIRE( fp.empty() );
    REQUIRE_EQ( (size_t)0, fp.size() );
    REQUIRE_EQ( string(), (string)fp );
    REQUIRE( ! fp.implementation().getOwns() );
}

TEST_CASE( Construct_String )
{
    string path = "/a/b/c/filename.ext";
    FilePath_<PlatformFilePath> fp( path );
    REQUIRE( ! fp.empty() );
    REQUIRE_EQ( path, (string)fp );
    REQUIRE_EQ( path.size(), fp.size() );
    REQUIRE( ! fp.implementation().getOwns() );
}

TEST_CASE( CopyConstruct_Empty )
{
    FilePath_<PlatformFilePath> fp0;
    FilePath_<PlatformFilePath> fp( fp0 );
    REQUIRE( fp.empty() );
}

TEST_CASE( CopyConstruct )
{
    string path = "/a/b/c/filename.ext";
    FilePath_<PlatformFilePath> fp0( path );

    FilePath_<PlatformFilePath> fp( fp0 );

    REQUIRE( ! fp.empty() );
    REQUIRE_EQ( path, (string)fp );
    REQUIRE_EQ( path.size(), fp.size() );
    REQUIRE( ! fp.implementation().getOwns() );
}

TEST_CASE( Assign_Empty )
{
    FilePath_<PlatformFilePath> fp0;
    FilePath_<PlatformFilePath> fp;
    fp = fp0;
    REQUIRE( fp.empty() );
}

TEST_CASE( Assign )
{
    string path = "/a/b/c/filename.ext";
    FilePath_<PlatformFilePath> fp0( path );

    FilePath_<PlatformFilePath> fp( string("qq") );
    fp = fp0;

    REQUIRE( ! fp.empty() );
    REQUIRE_EQ( path, (string)fp );
    REQUIRE_EQ( path.size(), fp.size() );
    REQUIRE( ! fp.implementation().getOwns() );
}

TEST_CASE( Copy )
{
    string path = "/a/b/c/filename.ext";
    FilePath_<PlatformFilePath> fp0( path );

    FilePath_<PlatformFilePath> fp = fp0.copy();

    REQUIRE_EQ( path, (string)fp );
    // copy() creates an owning object
    REQUIRE( fp.implementation().getOwns() );
}

TEST_CASE( Canonical_empty )
{
    REQUIRE_THROW( FilePath_<PlatformFilePath>().canonical() );
}

TEST_CASE( Canonical )
{
    FilePath_<PlatformFilePath> fp = FilePath_<PlatformFilePath>(".").canonical();
    REQUIRE_NE( string("."), (string)fp ); //TODO: what else can we verify?
    // canonical() creates an owning object, should be transferred by the copy constructor
    REQUIRE( fp.implementation().getOwns() );
}

TEST_CASE( Split )
{
    FilePath_<PlatformFilePath> fp( "/a/b/c/filename.ext" );
    auto p = fp.split();
    REQUIRE_EQ( (string)p.first, string("/a/b/c") );
    REQUIRE_EQ( (string)p.second, string("filename.ext") );
}

TEST_CASE( BaseName )
{
    FilePath_<PlatformFilePath> fp( "/a/b/c/filename.ext" );
    REQUIRE_EQ( string("filename.ext"), (string)fp.baseName() );
}

TEST_CASE( Append )
{
    FilePath_<PlatformFilePath> fp0( "/a/b" );
    FilePath_<PlatformFilePath> fp1( "c/filename.ext" );
    REQUIRE_EQ( string("/a/b/c/filename.ext"), (string) ( fp0.append( fp1 ) ) );
}

TEST_CASE( RemoveSuffix_NotOwned )
{
    FilePath_<PlatformFilePath> fp( "/a/b/c/filename.ext" );
    // removeSuffix requires ownership
    REQUIRE( ! fp.removeSuffix( 3 ) );
}

TEST_CASE( RemoveSuffix_0 )
{
    FilePath_<PlatformFilePath> fp0( "/a/b/c/filename.ext" );
    FilePath_<PlatformFilePath> fp = fp0.copy(); // creates an owning copy
    REQUIRE( ! fp.removeSuffix( 0 ) );
}

TEST_CASE( RemoveSuffix )
{
    FilePath_<PlatformFilePath> fp0( "/a/b/c/filename.ext" );
    FilePath_<PlatformFilePath> fp = fp0.copy();
    REQUIRE( fp.removeSuffix( 4 ) );
    REQUIRE_EQ( string("/a/b/c/filename"), (string)fp );
}

TEST_CASE( RemoveSuffix_Oversized )
{
    FilePath_<PlatformFilePath> fp0( "/a/b/c/filename.ext" );
    FilePath_<PlatformFilePath> fp = fp0.copy();
    REQUIRE( ! fp.removeSuffix( 1 + strlen("filename.ext") ) );
}

TEST_CASE( RemoveSuffixExact_NotOwned )
{
    FilePath_<PlatformFilePath> fp( "/a/b/c/filename.ext" );
    // removeSuffix requires ownership
    REQUIRE( ! fp.removeSuffix( "ext", 3 ) );
}

TEST_CASE( RemoveSuffixExact_NotFound )
{
    FilePath_<PlatformFilePath> fp0( "/a/b/c/filename.ext" );
    FilePath_<PlatformFilePath> fp = fp0.copy();
    REQUIRE( ! fp.removeSuffix( "oxt", 3 ) );
}

TEST_CASE( RemoveSuffixExact )
{
    FilePath_<PlatformFilePath> fp0( "/a/b/c/filename.ext" );
    FilePath_<PlatformFilePath> fp = fp0.copy();
    REQUIRE( fp.removeSuffix( ".ext", 4 ) );
    REQUIRE_EQ( string("/a/b/c/filename"), (string)fp );
}

TEST_CASE( RemoveSuffixString )
{
    FilePath_<PlatformFilePath> fp0( "/a/b/c/filename.ext" );
    FilePath_<PlatformFilePath> fp = fp0.copy();
    REQUIRE( fp.removeSuffix( string(".ext") ) );
    REQUIRE_EQ( string("/a/b/c/filename"), (string)fp );
}
TEST_CASE( RemoveSuffixFilePath )
{
    FilePath_<PlatformFilePath> fp0( "/a/b/c/filename.ext" );
    FilePath_<PlatformFilePath> fp = fp0.copy();
    FilePath_<PlatformFilePath> fp_suff ( ".ext" );
    REQUIRE( fp.removeSuffix( fp_suff ) );
    REQUIRE_EQ( string("/a/b/c/filename"), (string)fp );
}

TEST_CASE( CWD )
{
    FilePath_<PlatformFilePath> fp = FilePath_<PlatformFilePath>::cwd();
    REQUIRE( fp.readable() );
    REQUIRE( fp.implementation().getOwns() );
}

TEST_CASE( Exists )
{
    FilePath_<PlatformFilePath> fp = FilePath_<PlatformFilePath>::cwd();
    REQUIRE( fp.exists() );
    FilePath_<PlatformFilePath> fp1( "notafileatall" );
    FilePath_<PlatformFilePath> fp2 = fp.append( fp1 );
    REQUIRE( ! fp2.exists() );
    REQUIRE( ! fp2.readable() );
}

TEST_CASE( Exists_Static )
{
    FilePath_<PlatformFilePath> fp ( "CMakeLists.txt" );
    REQUIRE( FilePath_<PlatformFilePath>::exists( fp ) );
}

TEST_CASE( Readable )
{
    FilePath_<PlatformFilePath> fp = FilePath_<PlatformFilePath>::cwd();
    FilePath_<PlatformFilePath> fp1 ( "CMakeLists.txt" );
    FilePath_<PlatformFilePath> fp2 = fp.append( fp1 );
    REQUIRE( fp2.exists() );
    REQUIRE( fp2.readable() );
}

TEST_CASE( Executable )
{
    FilePath_<PlatformFilePath> fp = FilePath_<PlatformFilePath>::cwd();
    REQUIRE( fp.executable() );
    FilePath_<PlatformFilePath> fp1 ( "CMakeLists.txt" );
    FilePath_<PlatformFilePath> fp2 = fp.append( fp1 );
    REQUIRE( fp2.exists() );
    REQUIRE( ! fp2.executable() );
}

//TODO: USE_WIDE_API
//TODO: MS_Visual_C

//////////////////////////////////////////// Main
#include <kapp/args.h>
#include <kfg/config.h>

extern "C"
{

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}

rc_t CC Usage ( const Args * args )
{
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();

    return FilePathSuite(argc, argv);
}

}
