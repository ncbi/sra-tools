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

#pragma once

#include <string>
#include "util.hpp"
#include "file-path.hpp"
#include "command-line.hpp"

#include "proc.win32.hpp"
#include "proc.posix.hpp"

enum DebugPrintResult {
    dpr_Continue,
    dpr_Exit,       // exit unconditionally
    dpr_ExitIfChild // on Windows, this means "don't run the child"
};

/// @brief Print debugging information depending on testing level.
///
/// On POSIX, this is run in the child process, after fork and before exec. It does actually show what the environment contains in the child process.
/// On Windows, this needs to run before CreateProcess. It *CAN NOT* show what the environment contains in the child process.
DebugPrintResult debugPrintDryRun(FilePath const &toolPath, std::string const &toolName, char const *const *argv);
DebugPrintResult debugPrintDryRun(FilePath const &toolPath, std::string const &toolName, char const *cmdline);

template <typename IMPL>
struct Process_ : protected IMPL {
    template <typename ES_IMPL>
    struct ExitStatus_ : protected ES_IMPL {
        /// @brief Did the child process exit, e.g. as opposed to being ended by a signal.
        bool didExit() const {
            return ES_IMPL::didExit();
        }
        /// @brief The exit code if the child exited.
        int exitCode() const {
            return ES_IMPL::exitCode();
        }
        /// @brief Did the child exit normally by e.g. calling exit(0) or returning 0 from main.
        bool didExitNormally() const {
            return didExit() && exitCode() == 0;
        }
        operator bool() const { return didExitNormally(); }
        
        /// @brief Was the child process ended by a signal (as opposed to exiting).
        bool wasSignaled() const {
            return ES_IMPL::wasSignaled();
        }
        /// @brief The signal if the child process received one.
        int signal() const {
            assert(wasSignaled());
            return ES_IMPL::signal();
        }
        /// @brief The symbolic name of the signal that the child process received.
        char const *signalName() const {
            return ES_IMPL::signalName();
        }
        /// @brief Was a core dump was generated.
        bool didCoreDump() const {
            assert(wasSignaled());
            return ES_IMPL::coreDumped();
        }
        /// @brief Is the child stopped, e.g. in a debugger breakpoint
        bool isStopped() const {
            return ES_IMPL::stopped();
        }
        /// @brief The stop signal if the child process received one.
        int stopSignal() const {
            assert(isStopped());
            return ES_IMPL::stopSignal();
        }
        
        ExitStatus_(ExitStatus_ const &) = default;
        ExitStatus_ &operator =(ExitStatus_ const &) = default;
        ExitStatus_(ExitStatus_ &&) = default;
        ExitStatus_ &operator =(ExitStatus_ &&) = default;
    private:
        friend Process_;
        ExitStatus_(ES_IMPL value) : ES_IMPL(value) {}
    };
    using ExitStatus = ExitStatus_<typename IMPL::ExitStatus>;
    
    ExitStatus wait() const {
        return IMPL::wait();
    }

    static void runChild [[noreturn]] (FilePath const &toolPath, std::string const &toolName, char const *const *argv, Dictionary const &env = {})
    {
        IMPL::runChild(toolPath, toolName, argv, env);
    }
    static ExitStatus runChildAndWait(FilePath const &toolPath, std::string const &toolName, char const *const *argv, Dictionary const &env = {})
    {
        return IMPL::runChildAndWait(toolPath, toolName, argv, env);
    }
#if USE_WIDE_API
    static void runChild [[noreturn]] (FilePath const &toolPath, std::string const &toolName, wchar_t const *const *argv, Dictionary const &env = {})
    {
        IMPL::runChild(toolPath, toolName, argv, env);
    }
    static ExitStatus runChildAndWait(FilePath const &toolPath, std::string const &toolName, wchar_t const *const *argv, Dictionary const &env = {})
    {
        return IMPL::runChildAndWait(toolPath, toolName, argv, env);
    }
#endif

private:
#if USE_WIDE_API
    using API_Char = wchar_t;
    static API_Char const *const *args(CommandLine const &cmd) { return cmd.wargv; }
    static API_Char const *helpParameter() { return L"--help"; }
    static API_Char const *versionParameter(std::string const &toolName) {
        return toolName == "fastq-dump" ? L"-V" : L"--version";
    }
#else
    using API_Char = char;
    static API_Char const *const *args(CommandLine const &cmd) { return cmd.argv; }
    static API_Char const *helpParameter() { return "--help"; }
    static API_Char const *versionParameter(std::string const &toolName) {
        return toolName == "fastq-dump" ? "-V" : "--version";
    }
#endif
public:
    
    static ExitStatus runTool(CommandLine const &cmd, UniqueOrderedList<int> const &skip, Dictionary const &env)
    {
        auto j = skip.begin();
        auto const srcArgs = args(cmd);
        auto args = std::vector<API_Char const *>();
        
        args.reserve(cmd.argc + 1);
        args.push_back(cmd.runAs());
        
        for (auto i = 1; i < cmd.argc; ++i) {
            if (j == skip.end() || i != *j)
                args.push_back(srcArgs[i]);
            else
                ++j;
        }
        args.push_back(nullptr);
        
        return runChildAndWait(cmd.toolPath, cmd.toolName, args.data(), env);
    }
    static void execVersion [[noreturn]] (CommandLine const &cmd)
    {
        API_Char const *args[] = {
            cmd.runAs(),
            versionParameter(cmd.toolName),
            nullptr
        };
        runChild(cmd.toolPath, cmd.toolName, args);
    }
    static void execHelp [[noreturn]] (CommandLine const &cmd)
    {
        API_Char const *args[] = {
            cmd.runAs(),
            helpParameter(),
            nullptr
        };
        runChild(cmd.toolPath, cmd.toolName, args);
    }
    static void reexec [[noreturn]] (CommandLine const &cmd)
    {
        auto const srcArgs = args(cmd);
        auto args = std::vector<API_Char const *>();
        
        args.reserve(cmd.argc + 1);
        args.push_back(cmd.runAs());
        
        for (auto i = 1; i < cmd.argc; ++i) {
            args.push_back(srcArgs[i]);
        }
        args.push_back(nullptr);
        
        runChild(cmd.toolPath, cmd.toolName, args.data());
    }
    Process_(Process_ const &) = default;
    Process_ &operator =(Process_ const &) = default;
    Process_(Process_ &&) = default;
    Process_ &operator =(Process_ &&) = default;
};

using Process = Process_<PlatformProcess>;
