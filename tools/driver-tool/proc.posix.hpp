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
#include <vector>
#include <map>

#include <signal.h>
#include <sys/wait.h>

#include "parse_args.hpp"
#include "util.hpp"

extern char **environ; // why!!!

namespace sratools {

struct process {
    bool is_self() const { return pid == 0; }
    pid_t get_pid() const { return pid; }
    
    /// @brief the result of wait if child did terminate in some way
    struct exit_status {
        /// @brief child called exit
        bool exited() const {
            return WIFEXITED(value) ? true : false;
        }
        /// @brief child exit code
        ///
        /// Only available exited() == true.
        ///
        /// @return The low 8-bits of the value passed to exit.
        int exit_code() const {
            assert(exited());
            return WEXITSTATUS(value);
        }

        /// @brief child was signaled
        bool signaled() const {
            return WIFSIGNALED(value) ? true : false;
        }
        /// @brief the signal that terminated the child
        ///
        /// Only available signaled() == true
        ///
        /// @return the signal that terminated the child
        int termsig() const {
            assert(signaled());
            return WTERMSIG(value);
        }
        /// @brief the symbolic name of the signal that terminated the child
        ///
        /// Only available signaled() == true
        ///
        /// @return the symbolic name of the signal that terminated the child
        char const *termsigname() const {
            // list taken from https://pubs.opengroup.org/onlinepubs/009695399/basedefs/signal.h.html
            // removed entries that were not defined on CentOS Linux release 7.8.2003
            switch (termsig()) {
            case SIGHUP   : return "SIGHUP";
            case SIGINT   : return "SIGINT";
            case SIGQUIT  : return "SIGQUIT";
            case SIGILL   : return "SIGILL";
            case SIGTRAP  : return "SIGTRAP";
            case SIGABRT  : return "SIGABRT";
            case SIGBUS   : return "SIGBUS";
            case SIGFPE   : return "SIGFPE";
            case SIGKILL  : return "SIGKILL";
            case SIGUSR1  : return "SIGUSR1";
            case SIGSEGV  : return "SIGSEGV";
            case SIGUSR2  : return "SIGUSR2";
            case SIGPIPE  : return "SIGPIPE";
            case SIGALRM  : return "SIGALRM";
            case SIGTERM  : return "SIGTERM";
            case SIGCHLD  : return "SIGCHLD";
            case SIGCONT  : return "SIGCONT";
            case SIGSTOP  : return "SIGSTOP";
            case SIGTSTP  : return "SIGTSTP";
            case SIGTTIN  : return "SIGTTIN";
            case SIGTTOU  : return "SIGTTOU";
            case SIGURG   : return "SIGURG";
            case SIGXCPU  : return "SIGXCPU";
            case SIGXFSZ  : return "SIGXFSZ";
            case SIGVTALRM: return "SIGVTALRM";
            case SIGPROF  : return "SIGPROF";
            case SIGWINCH : return "SIGWINCH";
            case SIGIO    : return "SIGIO";
            case SIGSYS   : return "SIGSYS";
            default:
                return nullptr;
            }
        }
        /// @brief a coredump was generated
        ///
        /// Only available signaled() == true
        ///
        /// @return true if a coredump was generated
        bool coredump() const {
            assert(signaled());
            return WCOREDUMP(value) ? true : false;
        }

        /// @brief the child is stopped, like in a debugger
        bool stopped() const {
            return WIFSTOPPED(value) ? true : false;
        }
        /// @brief the signal that stopped the child
        ///
        /// Only available stopped() == true
        ///
        /// @return the signal that stopped the child
        int stopsig() const {
            assert(stopped());
            return WSTOPSIG(value);
        }

        /// @brief did child exit(0)
        bool normal() const {
            return exited() && exit_code() == 0;
        }
        operator bool() const {
            return normal();
        }

        exit_status(exit_status const &other) : value(other.value) {}
        exit_status &operator =(exit_status const &other) {
            value = other.value;
            return *this;
        }
    private:
        int value;
        friend struct process;
        exit_status(int status) : value(status) {}
    };

      /// @brief wait for this process to finish
      /// @return exit status
      /// @throw system_error if wait fails
    exit_status wait() const;

    static void run_child(char const *toolpath, char const *toolname, char const **argv, Dictionary const &env = {});
    static exit_status run_child_and_wait(char const *toolpath, char const *toolname, char const **argv, Dictionary const &env = {});
    static exit_status run_child_and_get_stdout(std::string *out, char const *toolpath, char const *toolname, char const **argv, bool const for_real = false, Dictionary const &env = {});

    process(process const &other) : pid(other.pid) {}
    process &operator =(process const &other) {
        pid = other.pid;
        return *this;
    }
protected:
    static process self() { return process(0); }
    process(pid_t pid) : pid(pid) {}
    
    pid_t pid;
};

} // namespace sratools
