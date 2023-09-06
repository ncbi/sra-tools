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

#if WINDOWS
#else

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

#include "proc.posix.hpp"
#include "proc.hpp"

extern char **environ; // why!!!

/// @brief c++ and const-friendly wrapper
///
/// execve is declare to take non-const, but any modification
/// would only happen after this process was replaced.
/// So from the PoV of this process, execve doesn't modify its arguments.
static inline int execve(char const *path, char const *const *argv, char const *const *env = environ)
{
    return ::execve(path, (char **)((void *)argv), (char **)((void *)env));
}

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

/// @brief calls exec; does not return; no debugging or dry run
///
/// @param toolpath the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param toolname the user-centric name of the tool, e.g. fastq-dump
/// @param argv argv
///
/// @throw system_error if exec fails
static void exec_really [[noreturn]] (FilePath const &toolPath, std::string const &toolName, char const *const *argv)
{
    std::string const exepath = toolPath;
    execve(exepath.c_str(), argv);
    throw_system_error(std::string("failed to exec ")+toolName);
}

/// @brief calls exec; does not return
///
/// @param toolpath the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param toolname the user-centric name of the tool, e.g. fastq-dump
/// @param argv argv
///
/// @throw system_error if exec fails
static void exec [[noreturn]] (FilePath const &toolPath, std::string const &toolName, char const *const *const argv)
{
#if USE_DEBUGGER
    auto const envar = getenv("SRATOOLS_DEBUG_CMD");
    if (envar && *envar) {
        exec_debugger(envar, argv);
    }
#endif
    switch (debugPrintDryRun(toolPath, toolName, argv)) {
    case dpr_Continue:
        exec_really(toolPath, toolName, argv);
    default:
        exit(0);
    }
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

namespace POSIX {

#define CASE_SIGNAL(SIG) case SIG: return "" # SIG

char const *Process::ExitStatus::signalName() const {
    // list taken from https://pubs.opengroup.org/onlinepubs/009695399/basedefs/signal.h.html
    // removed entries that were not defined on CentOS Linux release 7.8.2003
    switch (signal()) {
    CASE_SIGNAL(SIGHUP);
    CASE_SIGNAL(SIGINT);
    CASE_SIGNAL(SIGQUIT);
    CASE_SIGNAL(SIGILL);
    CASE_SIGNAL(SIGTRAP);
    CASE_SIGNAL(SIGABRT);
    CASE_SIGNAL(SIGBUS);
    CASE_SIGNAL(SIGFPE);
    CASE_SIGNAL(SIGKILL);
    CASE_SIGNAL(SIGUSR1);
    CASE_SIGNAL(SIGSEGV);
    CASE_SIGNAL(SIGUSR2);
    CASE_SIGNAL(SIGPIPE);
    CASE_SIGNAL(SIGALRM);
    CASE_SIGNAL(SIGTERM);
    CASE_SIGNAL(SIGCHLD);
    CASE_SIGNAL(SIGCONT);
    CASE_SIGNAL(SIGSTOP);
    CASE_SIGNAL(SIGTSTP);
    CASE_SIGNAL(SIGTTIN);
    CASE_SIGNAL(SIGTTOU);
    CASE_SIGNAL(SIGURG);
    CASE_SIGNAL(SIGXCPU);
    CASE_SIGNAL(SIGXFSZ);
    CASE_SIGNAL(SIGVTALRM);
    CASE_SIGNAL(SIGPROF);
    CASE_SIGNAL(SIGWINCH);
    CASE_SIGNAL(SIGIO);
    CASE_SIGNAL(SIGSYS);
    default:
        return nullptr;
    }
}

#undef CASE_SIGNAL

Process::ExitStatus Process::wait() const
{
    assert(pid != 0); ///< you can't wait on yourself
    if (pid == 0)
        throw std::logic_error("you can't wait on yourself!");

    do { // loop if wait is interrupted
        auto status = int(0);
        auto const rc = waitpid_with_signal_forwarding(pid, &status);

        if (rc > 0) {
            assert(rc == pid);
            return ExitStatus(status); ///< normal return is here
        }

        assert(rc != 0); // only happens if WNOHANG is given
    } while (errno == EINTR);
    
    assert(errno != ECHILD); // you already waited on this!
    if (errno == ECHILD)
        throw std::logic_error("child process was already reaped");
    
    throw_system_error("waitpid failed");
}

void Process::runChild [[noreturn]] (::FilePath const &toolpath, std::string const &toolname, char const *const *argv, Dictionary const &env)
{
    for (auto && v : env) {
        setenv(v.first.c_str(), v.second.c_str(), 1);
    }
    exec(toolpath, toolname, argv);
}

Process::ExitStatus Process::runChildAndWait(::FilePath const &toolpath, std::string const &toolname, char const *const *argv, Dictionary const &env)
{
    auto const pid = ::fork();
    if (pid < 0)
        throw_system_error("fork failed");
    if (pid == 0) {
        runChild(toolpath, toolname, argv, env);
    }
    return Process(pid).wait();
}

#if 0
process::exit_status process::run_child_and_get_stdout(std::string *out, FilePath const &toolpath, char const *toolname, char const **argv, bool const for_real, Dictionary const &env)
{
    int fds[2];

    if (::pipe(fds) < 0)
        throw_system_error("pipe failed");

    auto const pid = ::fork();
    if (pid < 0)
        throw_system_error("fork failed");
    if (pid == 0) {
        close(fds[0]);

        auto const newfd = dup2(fds[1], 1);
        close(fds[1]);

        if (newfd < 0)
            throw_system_error("dup2 failed");

        for (auto && v : env) {
            setenv(v.first.c_str(), v.second.c_str(), 1);
        }
        if (for_real)
            exec_really(toolpath.get(), toolname, argv);
        else
            exec(toolpath.get(), toolname, argv);
    }
    close(fds[1]);

    *out = std::string();
    char buffer[4096];
    ssize_t nread = 0;

    for ( ; ; ) {
        while ((nread = ::read(fds[0], buffer, sizeof(buffer))) > 0) {
            out->append(buffer, nread);
        }
        if (nread == 0) {
            close(fds[0]);
            return process(pid).wait();
        }
        auto const error = error_code_from_errno();
        if (error != std::errc::interrupted)
            throw std::system_error(error, "read failed");
    }
}
#endif

} // namespace POSIX

#endif // !WINDOWS
