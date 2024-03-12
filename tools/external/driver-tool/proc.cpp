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

#include <iostream>
#include "proc.hpp"
#include "debug.hpp"
#include "util.hpp"
#include "constants.hpp"
#include "file-path.hpp"

enum struct PrintMode {
    pretty,
    oneline,
    json
};

static inline void debugPrintEnvVar(char const *const name, bool const continueline = false)
{
    auto const value = EnvironmentVariables::get(name);
    if (value) {
        std::string const &in = value.value();
        std::string str;
        
        str.reserve(in.size());
        for (auto & ch : in) {
            switch (ch) {
            case '\'':
                str.append("\\'");
                break;
            case '\\':
                str.append("\\\\");
                break;
            default:
                str.append(1, ch);
                break;
            }
        }
        std::cerr << name << "='" << str << "'" << (continueline ? " \\\n" : "\n");
    }
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
    (void)argv0; // more CL garbage
}

/// @brief Print a string, in quotes, with escaping.
/// @param os stream to write to.
/// @param str string to write.
/// @return the stream.
static std::ostream &jsonPrint(std::ostream &os, char const *str)
{
    assert(str);
    os << '"';
    for ( ; ; ) {
        auto const ch = *str++;
        switch (ch) {
        case '\0':
            return os << '"';
        case '\\':
            os << "\\\\";
            break;
        case '"':
            os << "\\\"";
            break;
        case '\b':
            os << "\\b";
            break;
        case '\f':
            os << "\\f";
            break;
        case '\n':
            os << "\\n";
            break;
        case '\r':
            os << "\\r";
            break;
        case '\t':
            os << "\\t";
            break;
        default:
            os << (char)ch;
        }
    }
}

/// @brief Print a string, in quotes, with escaping.
/// @param os stream to write to.
/// @param str string to write.
/// @return the stream.
static std::ostream &jsonPrint(std::ostream &os, std::string const &str)
{
    return jsonPrint(os, str.c_str());
}

/// @brief Print a path, in quotes, with escaping.
/// @param os stream to write to.
/// @param str path to write.
/// @return the stream.
static std::ostream &jsonPrint(std::ostream &os, FilePath const &path)
{
    return jsonPrint(os, std::string(path));
}

/// @brief Print an object member (name-value pair)
/// @tparam T the value type
/// @param os stream to write to.
/// @param member the name-value pair to write.
/// @return the stream.
template <typename T>
static std::ostream &jsonPrint(std::ostream &os, char const *memberName, T const &value)
{
    return jsonPrint(jsonPrint(os, memberName) << ": ", value);
}

/// @brief Print an enviroment variable, if it is set.
/// @param os stream to write to.
/// @param name the name of the environment variable.
/// @param first is this the first member in the set (if not, it will need a separator).
/// @param indent number of tabs to indent.
/// @return the stream
static std::ostream &jsonPrintEnvVar(std::ostream &os, char const *name, bool &first)
{
    auto const value = EnvironmentVariables::get(name);
    if (value) {
        if (!first)
            os << ',';
        jsonPrint(os << "\n\t\t", name, value.value());
        first = false;
    }
    return os;
}

/// @brief Print the environment variables used.
/// @param os stream to write to.
/// @param indent number of tabs to indent.
/// @return the stream
static std::ostream &jsonPrintEnvVars(std::ostream &os)
{
    auto first = true;
    for (auto name : make_sequence(constants::env_var::names(), constants::env_var::END_ENUM))
        jsonPrintEnvVar(os, name, first);
    jsonPrintEnvVar(os, ENV_VAR_SESSION_ID, first);
    return os;
}

/// @brief Print the environment and command line arguments as JSON.
/// @param toolPath full path to executable
/// @param argv0 tool name
/// @param args arguments
static void testing_6(FilePath const &toolPath, char const *argv0, char const *const *const args)
{
    std::cerr << "{\n";
    jsonPrint(std::cerr << "\t", "executable-path", toolPath) << ",\n";
    jsonPrint(std::cerr << "\t\"command-line\": [\n\t\t", argv0);
    for (int i = 1; args[i]; ++i) {
        jsonPrint(std::cerr << ",\n\t\t", args[i]);
    }
    std::cerr << "\n\t],\n";

    std::cerr << "\t\"environment\": {";
    jsonPrintEnvVars(std::cerr);
    std::cerr << "\n\t}\n";

    std::cerr << "}\n";
}

/// @brief Print the environment and command line as JSON.
/// @param toolPath full path to executable.
/// @param cmdLine command line
static void testing_6(FilePath const &toolPath, char const *const cmdLine)
{
    std::cerr << "{\n";
    jsonPrint(std::cerr << "\t", "executable-path", toolPath) << ",\n";
    jsonPrint(std::cerr << "\t", "command-line", cmdLine) << ",\n";

    std::cerr << "\t\"environment\": {";
    jsonPrintEnvVars(std::cerr);
    std::cerr << "\n\t}\n";
    std::cerr << "}\n";
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

/// @brief Debug print command line.
/// @param toolPath full path to executable.
/// @param toolName tool name
/// @param argv arguments
/// @return what action should the caller take, depending on testing mode.
DebugPrintResult debugPrintDryRun(FilePath const &toolPath, std::string const &toolName, char const *const *argv)
{
    switch (logging_state::testing_level()) {
    case 6:
        testing_6(toolPath, argv[0], argv);
        return dpr_ExitIfChild;
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

/// @brief Debug print command line.
/// @param toolPath full path to executable.
/// @param toolName tool name
/// @param cmdLine command line
/// @return what action should the caller take, depending on testing mode.
DebugPrintResult debugPrintDryRun(FilePath const &toolPath, std::string const &toolName, char const *cmdline)
{
    switch (logging_state::testing_level()) {
    case 6:
        testing_6(toolPath, cmdline);
        return dpr_ExitIfChild;
    case 5:
        testing_5();
        return dpr_Exit;
    case 4:
        testing_4(toolPath, cmdline);
        return dpr_ExitIfChild;
    case 3:
        testing_3(toolPath, toolName.c_str(), cmdline);
        return dpr_ExitIfChild;
    case 2:
        testing_2(toolName, cmdline);
        return dpr_ExitIfChild;
    default:
        return dpr_Continue;
    }
}
