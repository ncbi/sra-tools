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

#if WINDOWS
#else

#include <string>
#include <vector>
#include <map>

#include <signal.h>
#include <sys/wait.h>
#include <sysexits.h>

#include "util.hpp"
#include "file-path.hpp"
#include "command-line.hpp"

namespace POSIX {

struct Process {
    /// @brief the result of wait if child did terminate in some way
    struct ExitStatus {
        /// @brief child called exit
        bool didExit() const {
            return WIFEXITED(value) ? true : false;
        }
        /// @brief child exit code
        ///
        /// Only available exited() == true.
        ///
        /// @return The low 8-bits of the value passed to exit.
        int exitCode() const {
            return WEXITSTATUS(value);
        }

        /// @brief child was signaled
        bool wasSignaled() const {
            return WIFSIGNALED(value) ? true : false;
        }
        /// @brief the signal that terminated the child
        ///
        /// Only available signaled() == true
        ///
        /// @return the signal that terminated the child
        int signal() const {
            return WTERMSIG(value);
        }
        /// @brief the symbolic name of the signal that terminated the child
        ///
        /// Only available signaled() == true
        ///
        /// @return the symbolic name of the signal that terminated the child
        char const *signalName() const;
        
        /// @brief a coredump was generated
        ///
        /// Only available signaled() == true
        ///
        /// @return true if a coredump was generated
        bool didCoreDump() const {
            return WCOREDUMP(value) ? true : false;
        }

        /// @brief the child is stopped, like in a debugger
        bool isStopped() const {
            return WIFSTOPPED(value) ? true : false;
        }
        /// @brief the signal that stopped the child
        ///
        /// Only available stopped() == true
        ///
        /// @return the signal that stopped the child
        int stopSignal() const {
            return WSTOPSIG(value);
        }

        ExitStatus(ExitStatus const &) = default;
        ExitStatus &operator =(ExitStatus const &) = default;
        ExitStatus(ExitStatus &&) = default;
        ExitStatus &operator =(ExitStatus &&) = default;
    private:
        int value;
        friend struct Process;
        ExitStatus(int status) : value(status) {}
    };

      /// @brief wait for this process to finish
      /// @return exit status
      /// @throw system_error if wait fails
    ExitStatus wait() const;

    static void runChild [[noreturn]] (::FilePath const &toolpath, std::string const &toolname, char const *const *argv, Dictionary const &env);
    static ExitStatus runChildAndWait(::FilePath const &toolpath, std::string const &toolname, char const *const *argv, Dictionary const &env);

    Process(Process const &) = default;
    Process &operator =(Process const &) = default;
    Process(Process &&) = default;
    Process &operator =(Process &&) = default;

    static Process self() { return Process(0); }
    Process(pid_t pid) : pid(pid) {}
private:
    pid_t pid;
};

} // namespace POSIX

using PlatformProcess = POSIX::Process;

#endif // ! WINDOWS
