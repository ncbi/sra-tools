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
#define USE_DEBUGGER 0
#endif

#include <string>
#include <vector>
#include <iostream>

#if USE_DEBUGGER
#include <sstream>
#endif

#include <cstdlib>

#include <unistd.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "debug.hpp"
#include "proc.hpp"
#include "globals.hpp"
#include "util.hpp"

namespace sratools {

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
    static auto const verbose = std::string("-v");
    auto const argc = 1                     // for argv[0]
                    + parameters.size() * 2 // probably an over-estimate
                    + arguments.size();
    auto const argv = new char const * [argc + 1];
    auto i = decltype(argc)(0);
    auto empty = std::string();
    
    argv[i++] = arg0.c_str();
    for (auto & param : parameters) {
        auto const &name = param.first != "--verbose" ? param.first : verbose;
        auto const &value = param.second;
        
        assert(i < argc);
        argv[i++] = name.c_str();
        if (value)
            argv[i++] = value.value().c_str();
    }
    for (auto & arg : arguments) {
        argv[i++] = arg.c_str();
    }
    argv[i] = NULL;
    
    return argv;
}

void exec [[noreturn]] (  std::string const &toolname
                        , std::string const &toolpath
                        , ParamList const &parameters
                        , ArgsList const &arguments)
{
    auto const argv = makeArgv(parameters, arguments);
    
    execve(toolpath.c_str(), argv);
    
    // NB. we should never get here
    auto const error = error_code_from_errno();

    delete [] argv; // probably pointless, but be nice anyways

    throw std::system_error(error, "failed to exec "+toolname);
}

#if USE_DEBUGGER
/// @brief run child tool in a debugger
///
/// @note uses $SHELL; escaping is super-primitive here, it just quotes all elements of argv
///
/// @example SRATOOLS_DEBUG_CMD="lldb --" SRATOOLS_IMPERSONATE=sam-dump sratools SRR000001 --output-file /dev/null
static void exec_debugger [[noreturn]] (  char const *debugger
                                        , char const * const *argv)
{
    auto const shell_envar = getenv("SHELL");
    auto const shell = (shell_envar && *shell_envar) ? shell_envar : "/bin/sh";
    auto oss = std::ostringstream();
    
    std::cerr << shell << " -c " << debugger;
    oss << debugger;
    for (auto arg = argv; *arg; ++arg) {
        std::cerr << " \"" << *arg << '"';
        oss << " \"" << *arg << '"';
    }
    std::cerr << std::endl;
    
    char const *new_argv[] = { shell, "-c", oss.str().c_str() };
    execve(shell, new_argv);
    
    auto const error = error_code_from_errno();
    throw std::system_error(error, "failed to exec debugger");
}
#endif

void exec [[noreturn]] (  std::string const &toolname
                        , std::string const &toolpath
                        , std::string const &argv0
                        , ParamList const &parameters
                        , ArgsList const &arguments)
{
    auto const argv = makeArgv(parameters, arguments, argv0);
    
#if USE_DEBUGGER
    auto const envar = getenv("SRATOOLS_DEBUG_CMD");
    if (envar && *envar) {
        exec_debugger(envar, argv);
    }
#endif
    execve(toolpath.c_str(), argv);
    
    // NB. we should never get here
    auto const error = error_code_from_errno();

    delete [] argv; // probably pointless, but be nice anyways

    throw std::system_error(error, "failed to exec "+toolname);
}

process::exit_status process::wait() const
{
    assert(pid != 0); ///< you can't wait on yourself
    if (pid == 0)
        throw std::logic_error("you can't wait on yourself!");

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
    
    throw_system_error("waitpid failed");
}

} // namespace sratools
