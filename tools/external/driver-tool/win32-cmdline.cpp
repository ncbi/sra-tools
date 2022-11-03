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
 *  provide Windows support
 *
 */

#if WINDOWS

#include "util.hpp"

#include <vector>
#include <cassert>
// using std::assert;

#include <shellapi.h>

#include "wide-char.hpp"
#include "win32-cmdline.hpp"

#if TESTING
#include <winbase.h> // for LocalFree
#include <iostream>
#include <initializer_list>
#include <vector>

namespace WIN32_CMDLINE_TESTS {
    using AutoFreeCommandLineToArgv = std::unique_ptr< wchar_t *, decltype(&LocalFree) >;
    std::pair< int, AutoFreeCommandLineToArgv> parseCmdLine(wchar_t const *cmdline)
    {
        int argc = 0;
        auto ptr = CommandLineToArgvW(cmdline, &argc);
        assert(ptr != nullptr);
        assert(ptr[argc] == nullptr);
        assert(ptr[argc - 1] != nullptr);
        return std::make_pair(argc, AutoFreeCommandLineToArgv(ptr, &LocalFree));
    }

    static void testCmdLineConvert(std::initializer_list<wchar_t const *> const &argv) {
        auto const args = std::vector<wchar_t const *>(argv);
#if WINDOWS
        using namespace Win32Support;
        auto const u = CmdLineString<wchar_t>(args.data());
        auto const &p = parseCmdLine(u.get());
        auto const parsedArgc = p.first;
        auto const parsedArgv = p.second.get();
#else
        auto const u = std::unique_ptr<wchar_t>(new wchar_t [1]); u.get()[0] = 0;
        auto const parsedArgc = args.size() - 1;
        auto const parsedArgv = args.data();
#endif

        if (args.size() - 1 == parsedArgc)
            ;
        else {
            std::cerr << "argc from CommandLineToArgvW (" << parsedArgc << ") does not match.\n";
            goto LOG_EXPECTED_AND_GOT;
        }

        for (auto i = 0; i < parsedArgc; ++i) {
            assert(args[i] != nullptr && parsedArgv[i] != nullptr);

            auto const in = std::wstring(args[i]);
            auto const parsed = std::wstring(parsedArgv[i]);

            if (in == parsed)
                continue;
#if WINDOWS
#else
            assert(!"reachable");
#endif
            std::wcerr << L"argv[" << i << L"] does not match.\n";
            std::wcerr << L"Expected:\t" << in << L'\n';
            std::wcerr << L"Got:\t" << parsed << L'\n' << std::endl;
            goto LOG_EXPECTED_AND_GOT;
        }
        return;

    LOG_EXPECTED_AND_GOT:
#if WINDOWS
        std::wcerr << L"Test failed for:\n\t" << u.get() << std::endl;
        std::wcerr << L"Expected: [\n";
        for (auto const &arg : args) {
            std::wcerr << L'\t' << arg << L'\n';
        }
        std::wcerr << L"]\nGot: [" << std::endl;
        for (auto i = 0; parsedArgv[i] != nullptr; ++i) {
            std::wcerr << L'\t' << parsedArgv[i] << L'\n';
        }
        std::wcerr << L"]\n" << std::endl;
        assert(!"good");
#else
        assert(!"reachable");
#endif
        abort();
    }
}
#endif /* TESTING */
#endif
