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

#include "parse_args.hpp"

extern char **environ; ///< why!!!

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
        
        // TODO: why is this needed?
        operator bool() const {
            return value != 0;
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
    
    /// @brief create a child process
    /// @return child pid to parent, pid 0 to child
    /// @throw system_error if fork fails
    static process fork() {
        auto const pid = ::fork();
        if (pid < 0)
            throw std::system_error(std::error_code(errno, std::system_category()), "fork failed");
        return process(pid);
    }
    
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

/// @brief a file descriptor and a process id
///
/// For the parent process, the file descriptor is readable, and the process id is the child's.
/// For the child process, the file descriptor is writable, and the process id is 0.
struct pipe_and_fork : public process {
    /// @brief the file descriptor for this process
    int fd() const { return pipe; }
    bool is_child() const { return pid == 0; }

    /// @brief make a pipe and fork
    static pipe_and_fork make();
    
    /// @brief make a pipe, fork, and redirect child stdout to pipe
    static pipe_and_fork to_stdout();
    
    pipe_and_fork(pipe_and_fork const &other) : pipe(other.pipe), process(other) {}
    pipe_and_fork &operator =(pipe_and_fork const &other) {
        pipe = other.pipe;
        return *this;
    }
private:
    pipe_and_fork(int pipe, process proc = process::self()) : pipe(pipe), process(proc) {}
    int pipe;
};


/// @brief c++ and const-friendly wrapper
///
/// execve is declare to take non-const, but any modification
/// would only happen after this process was replaced.
/// So from the PoV of this process, execve doesn't modify its arguments.
static inline int execve(char const *path, char const *const *argv, char const *const *env = environ)
{
    return ::execve(path, (char **)((void *)argv), (char **)((void *)env));
}


/// @brief prepares argv and calls exec; does not return
///
/// @param toolname the user-centric name of the tool, e.g. fastq-dump
/// @param toolpath the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param parameters list of parameters (name-value pairs)
/// @param arguments list of strings
///
/// @throw system_error if exec fails
extern
void exec [[noreturn]] (  std::string const &toolname
                        , std::string const &toolpath
                        , ParamList const &parameters
                        , ArgsList const &arguments);


/// @brief prepares argv and calls exec; does not return
///
/// @param toolname the user-centric name of the tool, e.g. fastq-dump
/// @param toolpath the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param parameters list of parameters (name-value pairs)
/// @param argument a string
///
/// @throw system_error if exec fails
static inline
void exec [[noreturn]] (  std::string const &toolname
                        , std::string const &toolpath
                        , ParamList const &parameters
                        , ArgsList::value_type const &argument)
{
    exec(toolname, toolpath, parameters, ArgsList({ argument }));
}

} // namespace sratools
