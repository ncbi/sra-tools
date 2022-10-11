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

/// source: https://github.com/openbsd/src/blob/master/include/sysexits.h
#define EX_USAGE    64    /* command line usage error */
#define EX_NOINPUT    66    /* cannot open input */
#define EX_SOFTWARE    70    /* internal software error */
#define EX_TEMPFAIL    75    /* temp failure; user is invited to retry */
#define EX_CONFIG    78    /* configuration error */

typedef int pid_t;

namespace Win32 {

struct Process {
    /// @brief the result of wait if child did terminate in some way
    struct ExitStatus {
        /// @brief does nothing; not really applicable to Windows; child processes always only exit
        ///
        /// @return true
        bool didExit() const {
            return true;
        }
        /// @brief child exit code
        int exitCode() const {
            return value;
        }
        /// @brief does nothing; not applicable to Windows; only for API compatibility with POSIX
        ///
        /// @return false
        bool wasSignaled() const {
            return false;
        }
        /// @brief does nothing; not applicable to Windows; only for API compatibility with POSIX
        ///
        /// @return 0
        int signal() const {
            return 0;
        }
        /// @brief does nothing; not applicable to Windows; only for API compatibility with POSIX
        ///
        /// @return nullptr
        char const *signalName() const {
            return "UNKNOWN";
        }
        /// @brief does nothing; not applicable to Windows; only for API compatibility with POSIX
        ///
        /// @return false
        bool didCoreDump() const {
            return false;
        }
        /// @brief does nothing; not applicable to Windows; only for API compatibility with POSIX
        bool isStopped() const {
            return false;
        }
        /// @brief does nothing; not applicable to Windows; only for API compatibility with POSIX
        ///
        /// @return 0
        int stopSignal() const {
            return 0;
        }

        ExitStatus(ExitStatus const &) = default;
        ExitStatus &operator =(ExitStatus const &);
        ExitStatus(ExitStatus &&) = default;
        ExitStatus &operator =(ExitStatus &&);
    private:
        int value;
        friend struct Process;
        ExitStatus(int status) : value(status) {}
    };

    static void runChild [[noreturn]] (::FilePath const &toolPath, std::string const &toolName, char const *const *argv, Dictionary const &env);
    static ExitStatus runChildAndWait(::FilePath const &toolPath, std::string const &toolName, char const *const *argv, Dictionary const &env);

#if USE_WIDE_API
    static void runChild [[noreturn]] (::FilePath const &toolPath, std::string const &toolName, wchar_t const *const *argv, Dictionary const &env);
    static ExitStatus runChildAndWait(::FilePath const &toolPath, std::string const &toolName, wchar_t const *const *argv, Dictionary const &env);
#endif
};

} // namespace Win32

using PlatformProcess = Win32::Process;

#else // WINDOWS
#endif // WINDOWS
