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
#define SYS_CHAR char
#define MAIN main
#else
#include "file-path.posix.cpp"
#define SYS_CHAR char
#define MAIN main
#endif
#include "build-version.cpp"
#include "command-line.cpp"

struct Test_CommandLine {
    /// Basic Test; can it parse?
    static void basic_test(int argc, SYS_CHAR *argv[], SYS_CHAR *envp[], char *extra[])
    {
        CommandLine cmdline(argc, argv, envp, extra);
        auto const toolName = std::string(cmdline.toolName);
        auto const expected = std::string{"Test_Drivertool_CommandLine"};

        if (toolName.substr(0, expected.length()) != expected)
            throw __FUNCTION__;
    }
    /// Can it detect that a re-exec would be a fork bomb?
    static void test_short_circuit(int argc, SYS_CHAR *argv[], SYS_CHAR *envp[], char *extra[])
    {
        // will not work on Windows in non-debug code
        // due to pollution of the implementation by
        // the driver tool.
#if DEBUG || _DEBUGGING
        CommandLine cmdline(argc, argv, envp, extra);

        if (!cmdline.isShortCircuit())
            throw __FUNCTION__;
#endif
    }

    Test_CommandLine(int argc, SYS_CHAR *argv[], SYS_CHAR *envp[], char *extra[])
    : is_good(false)
    {
        try {
            basic_test(argc, argv, envp, extra);
            test_short_circuit(argc, argv, envp, extra);
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

#if MAC
#define EXTRA extra
int MAIN ( int argc, SYS_CHAR **argv, SYS_CHAR **envp, char **extra )
#else
#define EXTRA nullptr
int MAIN ( int argc, SYS_CHAR **argv, SYS_CHAR **envp )
#endif
{
    try {
        auto const test = Test_CommandLine {argc, argv, envp, EXTRA};
        if (test.is_good)
            return 0;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 3;
}
