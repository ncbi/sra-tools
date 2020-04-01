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

#if DEBUG || _DEBUGGING
#define USE_DEBUGGER 1
#endif

#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <functional>
#include <atomic>

#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <signal.h>

#include "debug.hpp"
#include "proc.hpp"
#include "globals.hpp"
#include "util.hpp"
#include "env_vars.h"
#include "constants.hpp"

namespace sratools {

#if USE_DEBUGGER

/// @brief run child tool in a debugger
///
/// @note uses $SHELL; it squotes all elements of argv, escaping squote and backslash
///
/// @example With gdb: SRATOOLS_IMPERSONATE=sam-dump SRATOOLS_DEBUG_CMD="gdb --args" sratools SRR000001 --output-file /dev/null \par
/// With lldb: SRATOOLS_IMPERSONATE=sam-dump SRATOOLS_DEBUG_CMD="lldb --" sratools SRR000001 --output-file /dev/null
static void exec_debugger [[noreturn]] (  char const *const debugger
                                        , char const * const *const argv)
{
    auto const shell_envar = getenv("SHELL");
    auto const shell = (shell_envar && *shell_envar) ? shell_envar : "/bin/sh";
    auto cmd = std::string(debugger);
    
    for (auto arg = argv; ; ) {
        auto cp = *arg++;
        if (cp == nullptr)
            break;
        
        cmd += " '";
        for ( ; ; ) {
            auto const ch = *cp++;
            if (ch == '\0')
                break;
            if (ch == '\'' || ch == '\\') // these need to be escaped
                cmd += '\\';
            cmd += ch;
        }
        cmd += "'";
    }
    char const *new_argv[] = { shell, "-c", cmd.c_str(), nullptr };

    fprintf(stderr, "%s %s %s", new_argv[0], new_argv[1], new_argv[2]);
    execve(shell, new_argv);
    throw_system_error("failed to exec debugger");
}
#endif


static void debugPrintEnvVar(char const *const name, bool const continueline = false)
{
    auto const value = getenv(name);
    if (value)
        std::cerr << name << "='" << value << "'" << (continueline ? " \\\n" : "\n");
}

static void debugPrintDryRun(  char const *const toolpath
                             , char const *const toolname
                             , char const *const *const argv)
{
    switch (logging_state::testing_level()) {
    case 4:
        for (auto name : make_sequence(constants::env_var::names(), constants::env_var::END_ENUM)) {
            debugPrintEnvVar(name, true);
        }
        debugPrintEnvVar(ENV_VAR_SESSION_ID, true);
        std::cerr << toolpath;
        for (auto i = 1; argv[i]; ++i)
            std::cerr << ' ' << argv[i];
        std::cerr << std::endl;
        exit(0);
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
        exit(0);
        break;
    case 2:
        std::cerr << toolname;
        for (auto i = 1; argv[i]; ++i)
            std::cerr << ' ' << argv[i];
        std::cerr << std::endl;
        exit(0);
        break;
    default:
        break;
    }
}

void exec [[noreturn]] (char const *const toolpath,
                        char const *const toolname,
                        char const *const *const argv)
{
#if USE_DEBUGGER
    auto const envar = getenv("SRATOOLS_DEBUG_CMD");
    if (envar && *envar) {
        exec_debugger(envar, argv);
    }
#endif
    debugPrintDryRun(toolpath, toolname, argv);
    execve(toolpath, argv);
    throw_system_error(std::string("failed to exec ")+toolname);
}

void exec [[noreturn]] (  std::string const &toolname
                        , std::string const &toolpath
                        , std::string const &argv0
                        , ParamList const &parameters
                        , ArgsList const &arguments)
{
    auto const argc = 1                     // for argv[0]
                    + parameters.size() * 2 // probably an over-estimate
                    + arguments.size();
    auto const argv = (char const **)calloc(argc, sizeof(char const *));
    defer { free(argv); };
    {
        static auto const verbose = std::string("-v");
        auto i = decltype(argc)(0);
        auto const check_index = make_checker([&](){ return i < argc; });
        
        check_index(); argv[i++] = argv0.c_str();
        for (auto & param : parameters) {
            auto const &name = param.first != "--verbose" ? param.first : verbose;
            auto const &value = param.second;
            
            check_index(); argv[i++] = name.c_str();
            if (value) {
                check_index(); argv[i++] = value.value().c_str();
            }
        }
        for (auto & arg : arguments) {
            check_index(); argv[i++] = arg.c_str();
        }
        check_index();
    }
    exec(toolpath.c_str(), toolname.c_str(), argv);
}

static pid_t forward_target_pid;
static void sig_handler_for_waiting(int sig)
{
    kill(forward_target_pid, sig);
}

static int waitpid_with_signal_forwarding(pid_t const pid, int *const status)
{
    struct sigaction act, old;

    // we are not reentrant
    static std::atomic_flag lock = ATOMIC_FLAG_INIT;
    auto const was_locked = lock.test_and_set();
    assert(was_locked == false);
    if (was_locked)
        throw std::logic_error("NOT REENTRANT!!!");

    // set up signal forwarding
    forward_target_pid = pid;

    act.sa_handler = sig_handler_for_waiting;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    // set the signal handler
    if (sigaction(SIGINT, &act, &old) < 0)
        throw_system_error("sigaction failed");

    auto const rc = waitpid(pid, status, 0);

    // restore signal handler to old state
    if (sigaction(SIGINT, &old, nullptr))
        throw_system_error("sigaction failed");

    lock.clear();

    return rc;
}

process::exit_status process::wait() const
{
    assert(pid != 0); ///< you can't wait on yourself
    if (pid == 0)
        throw std::logic_error("you can't wait on yourself!");

    do {
        auto status = int(0);
        auto const rc = waitpid_with_signal_forwarding(pid, &status);

        if (rc > 0) {
            assert(rc == pid);
            return exit_status(status); ///< normal return is here
        }

        assert(rc != 0); // only happens if WNOHANG is given
        if (rc == 0)
            std::unexpected();
    } while (errno == EINTR);
    
    assert(errno != ECHILD); // you already waited on this!
    if (errno == ECHILD)
        throw std::logic_error("child process was already reaped");
    
    throw_system_error("waitpid failed");
}

} // namespace sratools
