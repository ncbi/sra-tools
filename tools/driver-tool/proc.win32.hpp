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

#include <sys/wait.h>

#include "parse_args.hpp"
#include "util.hpp"

namespace sratools {

struct process {
    bool is_self() const { return pid == 0; }
    pid_t get_pid() const { return pid; }
    
    /// @brief the result of wait if child did terminate in some way
    struct exit_status {
        /// @brief does nothing; returns true
        bool exited() const {
            return true;
        }
        /// @brief child exit code
        int exit_code() const {
            assert(exited());
            return value;
        }

        /// @brief does nothing; returns false
        bool signaled() const {
            return false;
        }
        /// @brief does nothing
        ///
        /// @return 0
        int termsig() const {
            assert(signaled());
            return 0;
        }
        /// @brief does nothing
        ///
        /// @return false
        bool coredump() const {
            assert(signaled());
            return false;
        }

        /// @brief does nothing; always false
        bool stopped() const {
            return false;
        }
        /// @brief does nothing
        ///
        /// @return 0
        int stopsig() const {
            assert(stopped());
            return 0;
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

    static void run_child(char const *toolpath, char const *toolname, char const **argv, Dictionary const &env = {});
    static exit_status run_child_and_wait(char const *toolpath, char const *toolname, char const **argv, Dictionary const &env = {});
    static exit_status run_child_and_get_stdout(std::string *out, char const *toolpath, char const *toolname, char const **argv, Dictionary const &env = {});
};

} // namespace sratools
