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
 *  process stuff, like CreateProcess
 *
 */

#if WINDOWS

#include "util.hpp"
#include <stdexcept>
#include <cassert>
#include <string>
#include <vector>
#include <map>

#include "util.win32.hpp"
#include "file-path.hpp"
#include "proc.hpp"
#include "proc.win32.hpp"
#include "wide-char.hpp"
#include "win32-cmdline.hpp"

extern std::string getPathA(FilePath const &in);
extern std::wstring getPathW(FilePath const &in);

#if USE_WIDE_API

static
DWORD runAndWait(FilePath const &toolpath, std::string const &toolname, wchar_t const *const *const argv, Dictionary const &env)
{
    auto const &wcmdline = Win32Support::CmdLineString<wchar_t>(argv);
    auto const &cmdline = Win32Support::makeUnwide(wcmdline.get());
    auto const dpr = debugPrintDryRun(toolpath, toolname, cmdline.get());

    if (dpr == dpr_Continue) {
        DWORD exitCode = 0;
        auto const restore = EnvironmentVariables::AutoRestore(env);

        PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
        STARTUPINFOW si; ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        si.dwFlags |= STARTF_USESTDHANDLES;

        auto const path = getPathW(toolpath);
        if (!CreateProcessW(path.c_str(), (LPWSTR)wcmdline.get(), NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi))
        {
            throw_system_error("CreateProcess");
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode;
    }
    if (dpr == dpr_Exit)
        ExitProcess(0);
    return 0;
}

#endif

static
DWORD runAndWait(FilePath const &toolpath, std::string const &toolname, char const *const *const argv, Dictionary const &env)
{
    auto const cmdline = Win32Support::CmdLineString<char>(argv);
    auto const dpr = debugPrintDryRun(toolpath, toolname, cmdline.get());

    if (dpr == dpr_Continue) {
        DWORD exitCode = 0;
        auto const restore = EnvironmentVariables::AutoRestore(env);

        PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
        STARTUPINFOA si; ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        si.dwFlags |= STARTF_USESTDHANDLES;

        auto const path = getPathA(toolpath);
        if (!CreateProcessA(path.c_str(), (LPSTR)cmdline.get(), NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi))
        {
            throw_system_error("CreateProcess");
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode;
    }
    if (dpr == dpr_Exit)
        ExitProcess(0);
    return 0;
}

namespace Win32 {

#if USE_WIDE_API

Process::ExitStatus Process::runChildAndWait(::FilePath const &toolpath, std::string const &toolname, wchar_t const *const *argv, Dictionary const &env)
{
    return ExitStatus(runAndWait(toolpath, toolname, argv, env));
}

void Process::runChild(::FilePath const &toolpath, std::string const &toolname, wchar_t const *const *argv, Dictionary const &env)
{
    ExitProcess(runAndWait(toolpath, toolname, argv, env));
}

#endif

Process::ExitStatus Process::runChildAndWait(::FilePath const &toolpath, std::string const &toolname, char const *const *argv, Dictionary const &env)
{
    return ExitStatus(runAndWait(toolpath, toolname, argv, env));
}

void Process::runChild(::FilePath const &toolpath, std::string const &toolname, char const *const *argv, Dictionary const &env)
{
    ExitProcess(runAndWait(toolpath, toolname, argv, env));
}

} // namespace Win32

#endif // WINDOWS
