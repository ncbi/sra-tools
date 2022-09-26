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

#include "proc.hpp"
#include "debug.hpp"
#include "util.hpp"
#include "constants.hpp"
#include "file-path.hpp"

static inline void debugPrintEnvVar(char const *const name, bool const continueline = false)
{
    auto const value = EnvironmentVariables::get(name);
    if (value)
        std::cerr << name << "='" << value << "'" << (continueline ? " \\\n" : "\n");
}

/// @brief Print names and values of our set environment variables.
static void printEnvironment(bool const continueline = false)
{
    for (auto name : make_sequence(constants::env_var::names(), constants::env_var::END_ENUM))
        debugPrintEnvVar(name, continueline);
    debugPrintEnvVar(ENV_VAR_SESSION_ID, continueline);
}

/// @brief Print the command line, as-if it had been typed by a user.
static void printCommandLine(char const *const argv0, char const *const *const argv)
{
    std::cerr << argv0;
    for (auto i = 1; argv[i]; ++i)
        std::cerr << ' ' << argv[i];
    std::cerr << std::endl;
}

/// @brief Print the command line, as-if it had been typed by a user.
static void printCommandLine(char const *const argv0, char const *const cmdline)
{
    std::cerr << cmdline << std::endl;
}

/// @brief Print the names of the environment variables that were set.
static void testing_5(void)
{
    for (auto name : make_sequence(constants::env_var::names(), constants::env_var::END_ENUM)) {
        auto const value = EnvironmentVariables::get(name);
        if (value)
            std::cerr << name << "\n";
    }
}

/// @brief Print the environment and command line. Potentially, the output could be pasted into a script and exec'ed.
template <typename T>
static void testing_4(std::string const &toolpath, T const args)
{
    printEnvironment(true);
    printCommandLine(toolpath.c_str(), args);
}

/// @brief Wordy print the command that would be executed along with the environment settings.
template <typename T>
static void testing_3(std::string const &toolpath, char const *argv0, T const args)
{
    std::cerr << "would exec '" << toolpath << "' as:\n";
    printCommandLine(argv0, args);

    std::cerr << "with environment:\n";
    printEnvironment();
    std::cerr << std::endl;
}

/// @brief Print the command line, as-if it had been typed by a user.
template <typename T>
static void testing_2(std::string const &toolname, T const args)
{
    printCommandLine(toolname.c_str(), args);
}

DebugPrintResult debugPrintDryRun(FilePath const &toolPath, std::string const &toolName, char const *const *argv)
{
    switch (logging_state::testing_level()) {
    case 5:
        testing_5();
        return dpr_Exit;
    case 4:
        testing_4(toolPath, argv);
        return dpr_ExitIfChild;
    case 3:
        testing_3(toolPath, argv[0], argv);
        return dpr_ExitIfChild;
    case 2:
        testing_2(toolName, argv);
        return dpr_ExitIfChild;
    default:
        return dpr_Continue;
    }
}
