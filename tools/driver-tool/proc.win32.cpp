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
 *  process stuff, like fork and exec
 *
 */

#include "proc.hpp"
#include "debug.hpp"
#include "constants.hpp"

static int countChars(char const *str, char chr)
{
    int n = 0;
    while (*str) {
        if (*str++ == chr)
            ++n;
    }
    return n;
}

static bool hasSpace(char const *str)
{
    while (*str) {
        auto const ch = *str++;
        if (ch == ' ' || ch == '\t')
            return true;
    }
    return false;
}

static wchar_t *create_command_line(char const **argv)
{
    wchar_t special[8];
    Win32Shim::widen(special, 8, " \\\"");

    size_t wcmdline_len = 0;
    int argc = 0;
    {
        int i;
        for (i = 0; ; ++i) {
            auto const arg = argv[i];
            if (arg == NULL) break;

            auto const slash = countChars(arg, '\\');
            auto const quote = countChars(arg, '"');
            auto const space = hasSpace(arg);
            wcmdline_len += slash + quote + (space ? 2 : 0);
            auto const wlen = Win32Shim::wideSize(arg);
            assert(wlen > 0);
            wcmdline_len += wlen;
        }
        argc = i;
    }
    assert(wcmdline_len > 0);
    auto const wcmdline = (wchar_t *)malloc( wcmdline_len * sizeof(wchar_t) ); // add 1 for nil-terminator
    assert(wcmdline != NULL);
    {
        auto buffer = wcmdline;
        auto remain = wcmdline_len;
        for (int i = 0; i < argc; ++i) {
            if (i > 0)
            {   // separating space
                *buffer++ = ' ';
                --remain; // originally was counted as the nil-terminator of the previous argument
            }

            auto str = argv[i];
            auto const space = hasSpace(str);
            if (space) {
                *buffer++ = special[2]; --remain; // append quote
            }
            for ( ; ; ) {
                int j = 0;
                while (str[j] != '\\' && str[j] != '"' && str[j] != '\0')
                    ++j;
                if (j > 0) {
                    auto const wlen = Win32Shim::widen(buffer, remain, str, j);
                    assert(wlen > 0);
                    buffer += wlen;
                    assert((ssize_t)remain > wlen);
                    remain -= wlen;
                }
                if (str[j] == '\0')
                    break;
                assert(remain > 2);
                *buffer++ = special[1];
                *buffer++ = special[str[j] == '\\' ? 1 : 2];
                remain -= 2;
                str = &str[j + 1];
            }
            if (space) {
                *buffer++ = special[2]; --remain; // append quote
            }
        }
        assert(remain == 1);
        *buffer++ = special[3]; // add the nil-terminator
    }
    return wcmdline;
}

PROCESS_INFORMATION createProcess(char const *toolpath, char const **argv, STARTUPINFOW *si)
{
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    {
        auto const wtoolpath = std::unique_ptr<wchar_t, decltype(deleterFree)>(Win32Shim::makeWide(toolpath), deleterFree);
        {
            auto const wcmdline = std::unique_ptr<wchar_t, decltype(deleterFree)>(create_command_line(argv), deleterFree);

            if (!CreateProcessW(wtoolpath.get(), wcmdline.get(), NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, si, &pi))
                throw_system_error("CreateProcess failed");
        }
    }
    return pi;
}

namespace sratools {

static bool debugPrintDryRun(char const *const toolpath
    , char const *const toolname
    , char const *const *const argv)
{
    switch (logging_state::testing_level()) {
    case 5:
        for (auto name : make_sequence(constants::env_var::names(), constants::env_var::END_ENUM)) {
            debugPrintEnvVarName(name);
        }
        exit(0);
    case 4:
        for (auto name : make_sequence(constants::env_var::names(), constants::env_var::END_ENUM)) {
            debugPrintEnvVar(name, true);
        }
        debugPrintEnvVar(ENV_VAR_SESSION_ID, true);
        std::cerr << toolpath;
        for (auto i = 1; argv[i]; ++i)
            std::cerr << ' ' << argv[i];
        std::cerr << std::endl;
        return true;
    case 3:
        std::cerr << "would exec '" << toolpath << "' as:\n";
        for (auto i = 0; argv[i]; ++i)
            std::cerr << ' ' << argv[i];
        {
            std::cerr << "\nwith environment:\n";
            for (auto name : make_sequence(constants::env_var::names(), constants::env_var::END_ENUM)) {
                debugPrintEnvVar(name);
            }
            debugPrintEnvVar(ENV_VAR_SESSION_ID);
            std::cerr << std::endl;
        }
        return true;
    case 2:
        {
            // remove "-orig"
            std::string tool = toolname;
            size_t pos = tool.find("-orig");
            if (pos != std::string::npos)
            {
                tool = tool.substr(0, pos);
            }

            std::cerr << tool;
            for (auto i = 1; argv[i]; ++i)
                std::cerr << ' ' << argv[i];
            std::cerr << std::endl;
            return true;
        }
    default:
        return false;
    }
}

void process::run_child(char const *toolpath, char const *toolname, char const **argv, Dictionary const &env)
{
    ExitProcess(run_child_and_wait(toolpath, toolname, argv, env).exit_code());
}

process::exit_status process::run_child_and_wait(char const *toolpath, char const *toolname, char const **argv, Dictionary const &env)
{
    if (debugPrintDryRun(toolpath, toolname, argv))
    {
        return 0;
    }

    STARTUPINFOW si; ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    auto const save = EnvironmentVariables::set_with_restore(env);
    auto const pi = createProcess(toolpath, argv, &si);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    EnvironmentVariables::restore(save);

    return exit_status(exitCode);
}

process::exit_status process::run_child_and_get_stdout(std::string *out, char const *toolpath, char const *toolname, char const **argv, bool const for_real, Dictionary const &env)
{
    SECURITY_ATTRIBUTES attr;
    ZeroMemory(&attr, sizeof(attr));
    attr.nLength = sizeof(attr);
    attr.bInheritHandle = TRUE;

    HANDLE fds[2];
    if (!CreatePipe(&fds[0], &fds[1], &attr, 0))
        throw_system_error("CreatePipe failed");

    if (!SetHandleInformation(fds[0], HANDLE_FLAG_INHERIT, 0))
        throw_system_error("SetHandleInformation failed");

    STARTUPINFOW si; ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = fds[1];
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    auto const save = EnvironmentVariables::set_with_restore(env);
    auto const pi = createProcess(toolpath, argv, &si);

    CloseHandle(fds[1]);

    *out = std::string();
    char buffer[4096];
    DWORD nread = 0;

    for ( ; ; ) {
        if (!ReadFile(fds[0], buffer, sizeof(buffer), &nread, NULL) || nread == 0) {
            break;
        }
        out->append(buffer, nread);
    }
    CloseHandle(fds[0]);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    EnvironmentVariables::restore(save);

    return exit_status(exitCode);
}

} // namespace sratools
