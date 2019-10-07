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
*  Communicate with SDL via srapath
*
*/

#include <string>
#include <vector>

#include "globals.hpp"
#include "constants.hpp"
#include "debug.hpp"
#include "proc.hpp"
#include "which.hpp"
#include "util.hpp"
#include "run-source.hpp"

using namespace constants;

namespace sratools {

static std::string run_srapath(std::string const &run)
{
    auto const toolname = std::string(tool_name::real(tool_name::SRAPATH));
    auto const toolpath_s = which_sratool(toolname);
    auto const toolpath = toolpath_s.c_str();
    char const *argv[] = {
        toolname.c_str(),
        "--function", "names",
        "--json",
        "--vers", resolver::version(),
        "--url", resolver::url(),
        NULL, NULL, ///< copy-paste this line to reserve space for more optional paramaters
        NULL,       // run goes here
        NULL        // argv is terminated
    };
    {
        auto constexpr argc = sizeof(argv) / sizeof(argv[0]);
        auto const argend = argv + argc;
        auto i = std::find_if(argv, argend, [](char const *arg) { return arg == NULL; });

        assert(i != argend && *i == NULL);
        if (location) {
            *i++ = "--location";
            assert(i != argend && *i == NULL);
            *i++ = location->c_str();
        }
        
        assert(i != argend && *i == NULL);
        *i++ = run.c_str();
        assert(i != argend && *i == NULL);
        
        if (logging_state::is_debug()) {
            std::cerr << toolpath_s << ": ";
            for (auto i = argv; i != argend && *i; ++i)
                std::cerr << *i << ' ';
            std::cerr << std::endl;
        }
    }
    auto fd = -1;
    auto const child = process::run_child_with_redirected_stdout(&fd, [&]() {
        execve(toolpath, argv);
        throw_system_error("failed to exec " + toolpath_s);
    });
    auto response = std::string();
    read_fd(fd, [&](char const *buffer, size_t nread) {
        response.append(buffer, nread);
    });
    TRACE(response);
    
    auto const result = child.wait();
    assert(result.signaled() || result.exited());
    if (result.signaled()) {
        std::cerr << "srapath (pid: " << child.get_pid() << ") was killed by signal " << result.termsig() << std::endl;
        std::unexpected();
    }
    if (result.exit_code() != 0) {
        std::cerr << "srapath exited with error code " << result.exit_code() << std::endl;
        exit(result.exit_code());
    }
    return response;
}

run_sources::run_sources(std::string const &qry)
{
    if (access(qry.c_str(), R_OK) == 0) {
        // it is a local file
        sources.push_back(run_source::local_file(qry));
        return;
    }
    auto response = run_srapath(qry);
    exit(0);
}
    
/// @brief set/unset CE Token environment variable
void run_sources::set_ce_token_env_var() const {
    if (have_ce_token)
        env_var::set(env_var::CE_TOKEN, ce_token_.c_str());
    else
        env_var::unset(env_var::CE_TOKEN);
}

} // namespace sratools
