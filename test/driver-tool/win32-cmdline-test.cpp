#if WIN32 || WIN64
#define WINDOWS 1
#include <Windows.h>
#define USE_WIDE_API 1
#endif

#include <iostream>

#define TESTING 1
#include "../../tools/driver-tool/win32-cmdline.cpp"
using namespace WIN32_CMDLINE_TESTS;

#if WINDOWS
int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
#else
int main(int argc, char *argv[], char *envp[])
#endif
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
}
