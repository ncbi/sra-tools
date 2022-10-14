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
 *  tests for command line parsing
 *
 */

#if WINDOWS
#include "file-path.win32.cpp"
#else
#include "file-path.posix.cpp"
#endif
#include "build-version.cpp"
#include "command-line.cpp"

struct Test_CommandLine {
    static void basic_test(int argc,
#if WINDOWS
        wchar_t *argv[], wchar_t *envp[],
#else
        char *argv[], char *envp[],
#endif
        char *extra[])
    {
        CommandLine cmdline(argc, argv, envp, extra);

        TRACE(cmdline.argv);
        TRACE(cmdline.envp);
        TRACE(cmdline.extra);
        TRACE(cmdline.fakeName ? cmdline.fakeName : "(not set)");
        TRACE(cmdline.baseName);
        TRACE(cmdline.toolName);
        TRACE((std::string)cmdline.fullPathToExe);
        TRACE((std::string)cmdline.fullPath);
        TRACE((std::string)cmdline.toolPath);
        TRACE(cmdline.realName);
    }

#if WINDOWS
    Test_CommandLine(int argc, wchar_t *argv[], wchar_t *envp[], char *extra[])
#else
    Test_CommandLine(int argc, char *argv[], char *envp[], char *extra[])
#endif
    : is_good(false)
    {
        try {
            basic_test(argc, argv, envp, extra);
            std::cerr << __FUNCTION__ << " passed." << std::endl;
            is_good = true;
        }
        catch (char const *func) {
            std::cerr << func << " failed!" << std::endl;
        }
        catch (...) {
            std::cerr << __FUNCTION__ << " failed: unknown exception!" << std::endl;
        }
    }

    bool is_good;
};

#if WINDOWS
int wmain ( int argc, wchar_t *argv[], wchar_t *envp[] )
#else
int main ( int argc, char *argv[], char *envp[] )
#endif
{
    try {
        auto const test = Test_CommandLine {argc, argv, envp, nullptr};
        if (test.is_good)
            return 0;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 3;
}
