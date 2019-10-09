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
#include "util.hpp"

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
        
        /// @brief did child exit(0)
        operator bool() const {
            return exited() && exit_code() == 0;
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
            throw_system_error("fork failed");
        return process(pid);
    }
    
    /// @brief fork and run closure in the child process
    ///
    /// @tparam F closure type, void []();
    /// @param f closure
    ///
    /// @return pid of child
    template <typename F>
    static process run_child(F && f)
    {
        auto const pid = ::fork();
        if (pid < 0)
            throw_system_error("fork failed");
        if (pid == 0) {
            f();
            assert(!"reachable");
            throw std::logic_error("child must not return");
        }
        return process(pid);
    }
    
    /// @brief make pipe and fork, and run closure in the child process
    ///
    /// @tparam F closure type, void [](int fd);
    /// @param f closure, recieves the writable end of the pipe
    /// @param fd is set to readable end of the pipe
    ///
    /// @return pid of child
    template <typename F>
    static process run_child_with_pipe(F && f, int *fd)
    {
        int fds[2];
        
        if (::pipe(fds) < 0)
            throw_system_error("pipe failed");
        
        auto const pid = ::fork();
        if (pid < 0)
            throw_system_error("fork failed");
        if (pid == 0) {
            close(fds[0]);
            f(fds[1]);
            assert(!"reachable");
            throw std::logic_error("child must not return");
        }
        *fd = fds[0];
        close(fds[1]);
        return process(pid);
    }
    
    template <typename F>
    static process run_child_with_redirected_stdout(int *fd, F && f)
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

            f();
            assert(!"reachable");
            throw std::logic_error("child must not return");
        }
        *fd = fds[0];
        close(fds[1]);
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
/// @param toolpath the full path to the tool, also uses for argv[0], e.g. /path/to/fastq-dump-orig
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
/// @param argv0 use this for argv[0]
/// @param parameters list of parameters (name-value pairs)
/// @param arguments list of strings
///
/// @throw system_error if exec fails
extern
void exec [[noreturn]] (  std::string const &toolname
                        , std::string const &toolpath
                        , std::string const &argv0
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


/// @brief prepares argv and calls exec; does not return
///
/// @param toolname the user-centric name of the tool, e.g. fastq-dump
/// @param toolpath the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param argv0 use this for argv[0]
/// @param parameters list of parameters (name-value pairs)
/// @param argument a string
///
/// @throw system_error if exec fails
static inline
void exec [[noreturn]] (  std::string const &toolname
                        , std::string const &toolpath
                        , std::string const &argv0
                        , ParamList const &parameters
                        , ArgsList::value_type const &argument)
{
    exec(toolname, toolpath, argv0, parameters, ArgsList({ argument }));
}

} // namespace sratools
