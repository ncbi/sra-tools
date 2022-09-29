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
#include "file-path.win32.cpp"

using namespace std;

#if WINDOWS
const char* A_B_C_Filename_Ext = "\\a\\b\\c\\filename.ext";
#else
const char* A_B_C_Filename_Ext = "/a/b/c/filename.ext";
#endif
const char* Posix_A_B_C_Filename_Ext = "/a/b/c/filename.ext";
const char* Posix_A_B_C_Filename= "/a/b/c/filename";

TEST_SUITE(FilePathSuite);

TEST_CASE( DefaultConstruct )
{
    FilePath fp;
    REQUIRE( fp.empty() );
    REQUIRE_EQ( (size_t)0, fp.size() );
    REQUIRE_EQ( string(), (string)fp );
}

TEST_CASE( Construct_Cstring )
{
    const char * path = A_B_C_Filename_Ext;
    FilePath fp( path );
    REQUIRE( ! fp.empty() );
    REQUIRE_EQ( string(Posix_A_B_C_Filename_Ext), (string)fp ); //NB: on Windows, conversion to string also converts separators to Posix
    REQUIRE_EQ( strlen( path ), fp.size() );
}

TEST_CASE( Construct_String )
{
    string path = A_B_C_Filename_Ext;
    FilePath fp( path );
    REQUIRE( ! fp.empty() );
    REQUIRE_EQ(string(Posix_A_B_C_Filename_Ext), (string)fp );
    REQUIRE_EQ( path.size(), fp.size() );
}

TEST_CASE( CopyConstruct_Empty )
{
    FilePath fp0;
    FilePath fp( fp0 );
    REQUIRE( fp.empty() );
}

TEST_CASE( CopyConstruct )
{
    string path = A_B_C_Filename_Ext;
    FilePath fp0( path );

    FilePath fp( fp0 );

    REQUIRE( ! fp.empty() );
    REQUIRE_EQ(string(Posix_A_B_C_Filename_Ext), (string)fp );
    REQUIRE_EQ( path.size(), fp.size() );
}

TEST_CASE( Assign_Empty )
{
    FilePath fp0;
    FilePath fp;
    fp = fp0;
    REQUIRE( fp.empty() );
}

TEST_CASE( Assign )
{
    string path = A_B_C_Filename_Ext;
    FilePath fp0( path );

    FilePath fp( string("qq") );
    fp = fp0;

    REQUIRE( ! fp.empty() );
    REQUIRE_EQ(string(Posix_A_B_C_Filename_Ext), (string)fp );
    REQUIRE_EQ( path.size(), fp.size() );
}

TEST_CASE( Copy )
{
    string path = A_B_C_Filename_Ext;
    FilePath fp0( path );

    FilePath fp = fp0.copy();

    REQUIRE_EQ(string(Posix_A_B_C_Filename_Ext), (string)fp );
    // copy() creates an owning object
}

TEST_CASE( Split )
{
    FilePath fp(A_B_C_Filename_Ext);
    auto p = fp.split();
    REQUIRE_EQ( (string)p.first, string("/a/b/c") );
    REQUIRE_EQ( (string)p.second, string("filename.ext") );
}

TEST_CASE( BaseName )
{
    FilePath fp(A_B_C_Filename_Ext);
    REQUIRE_EQ( string("filename.ext"), (string)fp.baseName() );
}

TEST_CASE( Append )
{
    FilePath fp0( "/a/b" );
    FilePath fp1( "c/filename.ext" );
    REQUIRE_EQ( string(Posix_A_B_C_Filename_Ext), (string) ( fp0.append( fp1 ) ) );
}

TEST_CASE( RemoveSuffix_0 )
{
    FilePath fp(A_B_C_Filename_Ext);
    REQUIRE( fp.removeSuffix( 0 ) );
}

TEST_CASE( RemoveSuffix )
{
    FilePath fp0(A_B_C_Filename_Ext);
    FilePath fp = fp0.copy();
    REQUIRE( fp.removeSuffix( 4 ) );
    REQUIRE_EQ( string(Posix_A_B_C_Filename), (string)fp );
}

TEST_CASE( RemoveSuffix_Oversized )
{
    FilePath fp(A_B_C_Filename_Ext);
    REQUIRE( ! fp.removeSuffix( 1 + fp.baseName().size() ) );
}

TEST_CASE( CWD )
{
    FilePath wd = FilePath::cwd();
    REQUIRE( wd.readable() );
}

TEST_CASE( Exists )
{
    FilePath fp = FilePath::cwd();
    REQUIRE( fp.exists() );
    FilePath fp1( "notafileatall" );
    FilePath fp2 = fp.append( fp1 );
    REQUIRE( ! fp2.exists() );
    REQUIRE( ! fp2.readable() );
}

TEST_CASE(CWD_Append)
{   // CWD can be wide on Windows
    FilePath fp = FilePath::cwd();
    FilePath fp1("CMakeLists.txt");
    FilePath fp2 = fp.append(fp1);
    REQUIRE(fp2.exists());
}

TEST_CASE( Exists_Static )
{
    REQUIRE( FilePath::exists( "CMakeLists.txt" ) );
}

TEST_CASE( Readable )
{
    FilePath fp = FilePath::cwd();
    FilePath fp1 ( "CMakeLists.txt" );
    FilePath fp2 = fp.append( fp1 );
    REQUIRE( fp2.exists() );
    REQUIRE( fp2.readable() );
    REQUIRE(!fp2.executable());
}

#if WIN32
static wchar_t **s_argv;
#else
static char **s_argv;
#endif

TEST_CASE(Current_Executable)
{
    FilePath fp = FilePath::fullPathToExecutable(s_argv, nullptr, nullptr);
    FilePath fp1(s_argv[0]);
    REQUIRE_EQ((string)fp.baseName(), (string)fp1.baseName());
}

TEST_CASE(Executable_Yes)
{
    FilePath fp(argv[0]);
    REQUIRE(fp.exists());
    REQUIRE(fp.executable());
}

#if WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    argv = argv;
    try {
        return FilePathSuite(argc, (char**)argv);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
}

