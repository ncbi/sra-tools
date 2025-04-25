#define TESTING 1

#if WIN32 || WIN64
#define WINDOWS 1

/// NB. We rely on a `ShellApi` function which only has a wide char version.
#define USE_WIDE_API 1

#include <Windows.h>
#include <iostream>
#include <initializer_list>
#include <vector>
#include <cassert>
#include "win32-cmdline.hpp"

using namespace Win32Support;

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
    auto const u = CmdLineString<wchar_t>(args.data());
    auto const &p = parseCmdLine(u.get());
    auto const parsedArgc = p.first;
    auto const parsedArgv = p.second.get();

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

        std::wcerr << L"argv[" << i << L"] does not match.\n";
        std::wcerr << L"Expected:\t" << in << L'\n';
        std::wcerr << L"Got:\t" << parsed << L'\n' << std::endl;
        goto LOG_EXPECTED_AND_GOT;
    }
    return;

LOG_EXPECTED_AND_GOT:
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
    abort();
}

int main(int argc, wchar_t *argv[], wchar_t *envp[])
{
    testCmdLineConvert({ L"foobar", nullptr });
    testCmdLineConvert({ L"\\foobar", nullptr });
    testCmdLineConvert({ L"foo\\bar", nullptr });
    testCmdLineConvert({ L"foo bar", nullptr });
    testCmdLineConvert({ L"foo", L"bar", nullptr });
    testCmdLineConvert({ L"foo", L"bar baz", nullptr });
    testCmdLineConvert({ L"foo", L"bar\" baz", nullptr });
    testCmdLineConvert({ L"foo", L"bar\\\" baz", nullptr });
    testCmdLineConvert({ L"foo", L"bar\\", nullptr });
    testCmdLineConvert({ L"foo", L"bar\\baz", nullptr });
    testCmdLineConvert({ L"foo", L"bar\\\"", nullptr });
    testCmdLineConvert({ L"foo", L"bar\\\"", L"baz\\\"", nullptr });

    std::cout << "All tests passed" << std::endl;
    return 0;

    (void)argc, (void)argv, (void)envp;
}
#else // !WINDOWS
int main(int argc, char *argv[], char *envp[])
{
    return 0;
}
#endif
