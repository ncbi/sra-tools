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
 *  Main entry point for tool and initial dispatch
 *
 */

// main is at the end of the file

#if __cplusplus < 201103L
#error c++11 or higher is needed
#else

#include <tuple>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <fstream>
#include <system_error>

#include <unistd.h>
#include <sys/stat.h>

#include "globals.hpp"
#include "constants.hpp"
#include "args-decl.hpp"
#include "parse_args.hpp"
#include "which.hpp"

extern char **environ; ///< why!!!

extern std::string split_basename(std::string &dirname);
extern std::string split_version(std::string &name, std::string const &default_version);

namespace sratools {

std::string const *argv0;
std::string const *selfpath;
std::string const *basename;
std::string const *version_string;

std::vector<std::string> const *args;
std::map<std::string, std::string> const *parameters;

std::string const *location = NULL;
std::string const *ngc = NULL;

using namespace constants;

/// @brief wrapper because execve is declare to take non-const, which isn't c++ friendly
static int execve(char const *path, char const *const *argv, char const *const *env = environ)
{
    return ::execve(path, (char **)((void *)argv), (char **)((void *)env));
}

/// @brief creates and fills out argv
///
/// @param parameters list of parameters (name-value pairs)
/// @param arguments list of strings
/// @param arg0  value for argv[0]; default value is from original argv[0]
///
/// @return array suitable for passing to execve; can delete [] if execve fails
static
char const * const* makeArgv(ParamList const &parameters , ArgsList const &arguments, std::string const &arg0 = *argv0)
{
    auto const argc = 1 + parameters.size() * 2 + arguments.size();
    auto const argv = new char const * [argc + 1];
    auto i = 0;
    auto empty = std::string();
    
    argv[i++] = arg0.c_str();
    for (auto & param : parameters) {
        auto const &name = param.first;
        auto const &value = param.second;
        
        argv[i++] = name.c_str();
        argv[i++] = value ? value.value().c_str() : "";
    }
    for (auto & arg : arguments) {
        argv[i++] = arg.c_str();
    }
    argv[i] = NULL;
    
    return argv;
}

/// @brief prepares argv and calls exec; does not return
///
/// @param toolname the user-centric name of the tool, e.g. fastq-dump
/// @param toolpath the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param parameters list of parameters (name-value pairs)
/// @param arguments list of strings
///
/// @throw system_error if exec fails
static
void exec [[noreturn]] (  std::string const &toolname
                        , std::string const &toolpath
                        , ParamList const &parameters
                        , ArgsList const &arguments)
{
    auto const argv = makeArgv(parameters, arguments);
    
    execve(toolpath.c_str(), argv);
    
    // NB. we should never get here
    auto const error = std::error_code(errno, std::system_category());

    delete [] argv;

    throw std::system_error(error, "failed to exec "+toolname);
}

struct process {
    bool is_self() const { return pid == 0; }
    pid_t get_pid() const { return pid; }
    
    struct exit_status {
        bool ifexited() const {
            return WIFEXITED(value) ? true : false;
        }
        int exit_code() const {
            assert(ifexited());
            return WEXITSTATUS(value);
        }

        bool ifsignaled() const {
            return WIFSIGNALED(value) ? true : false;
        }
        int termsig() const {
            assert(ifsignaled());
            return WTERMSIG(value);
        }
        bool coredump() const {
            assert(ifsignaled());
            return WCOREDUMP(value) ? true : false;
        }

        bool ifstopped() const {
            return WIFSTOPPED(value) ? true : false;
        }
        int stopsig() const {
            assert(ifstopped());
            return WSTOPSIG(value);
        }
        
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

    exit_status wait() const {
        assert(pid != 0); ///< you can't wait on yourself

        auto status = int(0);
        auto const rc = waitpid(pid, &status, 0);
        if (rc > 0) {
            assert(rc == pid);
            return exit_status(status);
        }

        assert(rc != 0); // only happens if WNOHANG is given
        if (rc == 0)
            std::unexpected();

        if (errno == EINTR)
            return wait();
        
        assert(errno != ECHILD); // you already waited on this!
        if (errno == ECHILD)
            throw std::logic_error("child process was already reaped");
        
        throw std::system_error(std::error_code(errno, std::system_category()), "waitpid failed");
    }
    
    static process fork() {
        auto const pid = ::fork();
        if (pid < 0)
            throw std::system_error(std::error_code(errno, std::system_category()), "fork failed");
        return process(pid);
    }
    
    static process self() {
        return process(0);
    }
    
    process(process const &other) : pid(other.pid) {}
    process &operator =(process const &other) {
        pid = other.pid;
        return *this;
    }
protected:
    process(pid_t pid) : pid(pid) {}
    
    pid_t pid;
};

struct pipe_and_fork : public process {
    int fd() const { return pipe; }
    bool is_child() const { return pid == 0; }

    /// @brief make a pipe and fork
    ///
    /// @returns to parent, child pid and readable fd; to child, pid is 0 and fd is writeable
    static pipe_and_fork make()
    {
        int fds[2];
        
        if (::pipe(fds) < 0)
            throw std::system_error(std::error_code(errno, std::system_category()), "pipe failed");
        
        auto const new_proc = process::fork();
        auto const my_pipe = fds[new_proc.is_self() ? 1 : 0];
        auto const other_pipe = fds[new_proc.is_self() ? 0 : 1];
        
        close(other_pipe);
        return pipe_and_fork(my_pipe, new_proc);
    }
    
    /// @brief make a pipe and fork, redirect child stdout to pipe
    ///
    /// @returns to parent, child pid and readable fd; to child, pid is 0 and fd is 1
    static pipe_and_fork to_stdout()
    {
        auto const &paf = make();
        if (paf.pid != 0)
            return paf;
        
        auto const oldfd = paf.pipe;
        auto const newfd = dup2(oldfd, 1);

        close(oldfd);
        if (newfd < 0)
            throw std::system_error(std::error_code(errno, std::system_category()), "dup2 failed");

        return pipe_and_fork(newfd);
    }
    
    pipe_and_fork(pipe_and_fork const &other) : pipe(other.pipe), process(other) {}
    pipe_and_fork &operator =(pipe_and_fork const &other) {
        pipe = other.pipe;
        return *this;
    }
private:
    pipe_and_fork(int pipe, process proc = process::self()) : pipe(pipe), process(proc) {}
    int pipe;
};

/// @brief get type from accession name
///
/// @param query the accession name
///
/// @return the 3rd character if name matches SRA accession pattern
static
int SRA_AccessionType(std::string const &query)
{
    int st = 0;
    int type = 0;
    int digits = 0;
    
    for (auto &i : query) {
        switch (st) {
            case 0:
                switch (i) {
                    case 'D':
                    case 'E':
                    case 'S':
                        st += 1;
                        break;
                    default:
                        return 0;
                }
                break;
            case 1:
                if (i != 'R') return false;
                st += 1;
                break;
            case 2:
                type = i;
                switch (i) {
                    case 'A':   // submitter
                    case 'P':   // Project
                    case 'R':   // Run data
                    case 'S':   // Sample
                    case 'X':   // eXperiment
                        st += 1;
                        break;
                    default:
                        return 0;
                }
                break;
            case 3:
                if (i == '.')
                    goto DONE;
                if (!('0' <= i && i <= '9'))
                    return false;
                digits += 1;
                break;
            default:
                assert(!"reachable");
                break;
        }
        if (0) {
DONE:
            break;
        }
    }
    return (6 <= digits && digits <= 9) ? type : 0;
}

static
ArgsList expandAll(ArgsList const &accessions)
{
    auto result = ArgsList();
    auto seen = std::set<std::string>();
    
    for (auto & acc : accessions) {
        if (seen.find(acc) != seen.end())
            continue;

        seen.insert(acc);

        // if readable then just try using it
        if (access(acc.c_str(), R_OK) != 0) {
            auto const type = SRA_AccessionType(acc);
            switch (type) {
                case 'R':
                    // it's a run, just use it
                    break;
                    
                case 'P':
                case 'S':
                case 'X':
                    // TODO: open container types
                    break;

                default:
                    // see if resolver has any clue
                    break;
            }
        }
        result.push_back(acc);
    }
    return result;
}

/// @brief runs tool on list of accessions
///
/// After args parsing, this is the called to do the meat of the work.
/// Accession can be any kind of SRA accession that can be resolved to runs.
///
/// @param toolname  the user-centric name of the tool, e.g. fastq-dump
/// @param toolpath the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param unsafeOutputFileParamName set if the output format is not appendable
/// @param extension file extension to use for output file, e.g. ".sam"
/// @param parameters list of parameters (name-value pairs)
/// @param accessions list of accessions to process
static void processAccessions(  std::string const &toolname
                              , std::string const &toolpath
                              , std::string const &unsafeOutputFileParamName
                              , std::string const &extension
                              , ParamList const &parameters
                              , ArgsList const &accessions
                              )
{
    auto const runs = expandAll(accessions);
    
    for (auto & run : runs) {
        auto const paf = pipe_and_fork::make();
    
        if (paf.is_child()) {
            exec(toolname, toolpath, parameters, runs);
        }
        auto const result = paf.wait();
    }
    exit(0);
}

/// @brief runs tool on list of accessions
///
/// After args parsing, this is the called for tools that do their own communication with SDL, e.g. srapath.
/// Accession can be any kind of SRA accession that can be resolved to runs.
///
/// @param toolname the user-centric name of the tool, e.g. fastq-dump
/// @param toolpath the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param parameters list of parameters (name-value pairs)
/// @param accessions list of accessions to process
static void processAccessionsNoSDL(  std::string const &toolname
                                   , std::string const &toolpath
                                   , ParamList const &parameters
                                   , ArgsList const &accessions
                                   )
{
    auto const runs = expandAll(accessions);
    exec(toolname, toolpath, parameters, runs);
}

/// @brief gets tool to print its help message; does not return
///
/// @param toolname friendly name of tool
/// @param toolpath path to tool
///
/// @throw system_error if exec fails
static void toolHelp [[noreturn]] (  std::string const &toolname
                                   , std::string const &toolpath)
{
    char const *argv[] = {
        "",
        "--help",
        NULL
    };

    argv[0] = argv0->c_str();
    execve(toolpath.c_str(), argv);
    throw std::system_error(std::error_code(errno, std::system_category()), "failed to exec "+toolname);
}


static void running_as_srapath [[noreturn]] ()
{
    auto const &toolname = tool_name::runas(tool_name::SRAPATH);
    auto const &toolpath = which_sratool(toolname);
    static Dictionary const long_args = {
        { "-f", "--function" },
        { "-t", "--timeout" },
        { "-a", "--protocol" },
        { "-e", "--vers" },
        { "-u", "--url" },
        { "-p", "--param" },
        { "-r", "--raw" },
        { "-j", "--json" },
        { "-d", "--project" },
        { "-c", "--cache" },
        { "-P", "--path" },
        { "-V", "--version" },
        { "-L", "--log-level" },
        { "-v", "--verbose" },
        { "-+", "--debug" },
        { "-h", "--help" },
        { "-?", "--help" },
    };
    static Dictionary const has_args = {
        { "--function", "REQUIRED" },
        { "--location", "REQUIRED" },
        { "--timeout", "REQUIRED" },
        { "--protocol", "REQUIRED" },
        { "--vers", "REQUIRED" },
        { "--url", "REQUIRED" },
        { "--param", "REQUIRED" },
        { "--log-level", "REQUIRED" },
        { "--debug", "REQUIRED" },
    };
    ParamList params;
    ArgsList accessions;
    
    if (parseArgs(&params, &accessions, long_args, has_args)) {
        processAccessionsNoSDL(toolname, toolpath, params, accessions);
    }
    else {
        toolHelp(toolname, toolpath);
    }
    exit(0);
}

static void running_as_prefetch [[noreturn]] ()
{
    exit(0);
}

static void running_as_fastq_dump [[noreturn]] ()
{
    exit(0);
}

static void running_as_tool [[noreturn]] (int const tool)
{
    exit(0);
}

static void running_as_self [[noreturn]] ()
{
    exit(0);
}

static void runas [[noreturn]] (int const tool)
{
    switch (tool) {
    case tool_name::SRAPATH:
        running_as_srapath();
        break;
            
    case tool_name::PREFETCH:
        running_as_prefetch();
        break;

    case tool_name::FASTQ_DUMP:
        running_as_fastq_dump();
        break;

    case tool_name::FASTERQ_DUMP:
    case tool_name::SAM_DUMP:
    case tool_name::SRA_PILEUP:
        running_as_tool(tool);
        break;

    default:
        running_as_self();
        break;
    }
    assert(!"reachable");
}

static void main [[noreturn]] (const char *cargv0, int argc, char *argv[])
{
    std::string const s_argv0(cargv0);
    std::string s_selfpath(cargv0)
              , s_basename(split_basename(s_selfpath))
              , s_version(split_version(s_basename, "2.10.0")); ///< this needs to be the version string and not a hardcoded value
    std::string s_location;
    auto s_args = loadArgv(argc, argv);
    
    // extract and remove --location from args
    for (auto i = s_args.begin(); i != s_args.end(); ) {
        bool found;
        std::string value;
        decltype(i) next;

        std::tie(found, value, next) = matched("--location", i, s_args.end());
        if (found) {
            s_location.assign(value);
            location = &s_location;
            i = s_args.erase(i, next);
            continue;
        }
        ++i;
    }
    
    // setup const globals
    argv0 = &s_argv0;
    selfpath = &s_selfpath;
    basename = &s_basename;
    version_string = &s_version;
    args = &s_args;

    // run the tool as specified by basename
    runas(tool_name::lookup_iid(basename->c_str()));
}

} // namespace sratools

int main(int argc, char *argv[])
{
    auto const impersonate = getenv("SRATOOLS_IMPERSONATE");
    auto const argv0 = impersonate ? impersonate : argv[0];

    sratools::main(argv0, argc - 1, argv + 1);
}
#endif // c++11
