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

#if __cplusplus < 201103L
#error c++11 or higher is needed
#else

#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <sysexits.h>

extern char **environ; ///< why!!!

#include "globals.hpp"
#include "constants.hpp"
#include "args-decl.hpp"

extern std::string split_basename(std::string &dirname);
extern std::string split_version(std::string &name, std::string const &default_version);

namespace sratools {

// main is at the end of the file

std::string const *selfpath;
std::string const *basename;
std::string const *version_string;

std::vector<std::string> const *args;
std::map<std::string, std::string> const *parameters;

std::string const *location = NULL;
std::string const *ngc = NULL;

using namespace constants;

using Dictionary = std::map<std::string, std::string>;

/// @brief: runs tool on list of accessions
///
/// After args parsing, this is the called to do the meat of the work.
/// Accession can be any kind of SRA accession that can be resolved to runs.
///
/// @param toolname: the user-centric name of the tool, e.g. fastq-dump
/// @param toolpath: the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param unsafeOutputFileParamName: set if the output format is not appendable
/// @param extension: file extension to use for output file, e.g. ".sam"
/// @param parameters: dictionary of parameters
/// @param accessions: list of accessions to process
static void processAccessions(  std::string const &toolname
                              , std::string const &toolpath
                              , std::string const &unsafeOutputFileParamName
                              , std::string const &extension
                              , Dictionary const &parameters
                              , std::vector<std::string> const &accessions
                              )
{
    exit(0);
}

/// @brief: runs tool on list of accessions
///
/// After args parsing, this is the called for tools that do their own communication with SDL, e.g. srapath.
/// Accession can be any kind of SRA accession that can be resolved to runs.
///
/// @param toolname: the user-centric name of the tool, e.g. fastq-dump
/// @param toolpath: the full path to the tool, e.g. /path/to/fastq-dump-orig
/// @param parameters: dictionary of parameters
/// @param accessions: list of accessions to process
static void processAccessionsNoSDL(  std::string const &toolname
                                   , std::string const &toolpath
                                   , Dictionary const &parameters
                                   , std::vector<std::string> const &accessions
                                   )
{
    exit(0);
}

static void pathHelp [[noreturn]] (std::string const &toolname, bool const isSraTools)
{
    std::cerr << "could not find " << toolname;
    if (isSraTools)
        std::cerr << "which is part of this software distribution";
    std::cerr << std::endl;
    std::cerr << "This can be fixed in one of the following ways:" << std::endl
              << "* adding " << toolname << " to the directory that contains this tool, i.e. " << *selfpath << std::endl
              << "* adding the directory that contains " << toolname << " to the PATH environment variable" << std::endl
              << "* adding " << toolname << " to the current directory" << std::endl
            ;
    exit(EX_CONFIG);
}

static std::string which(std::string const &toolname, bool const allowNotFound = false, bool const isSra = true)
{
    return "";
}

static void toolHelp [[noreturn]] (  std::string const &toolname
                                   , std::string const &toolpath
                                   )
{
    char *argv[] = {
        "",
        "--help",
        NULL
    };
    argv[0] = const_cast<char *>(toolname.c_str()); ///< argv is an array of char *
    execve(toolpath.c_str(), argv, environ);
}

static bool parseArgs(  Dictionary *parameters
                      , std::vector<std::string> *arguments
                      , Dictionary const &longNames
                      , Dictionary const &hasArg)
{
    std::vector<std::string> argv(*args); ///< this is a mutable copy
    
    while (!argv.empty()) {
        
    }
    return false;
}

static void running_as_srapath [[noreturn]] ()
{
    auto const toolname = tool_name::runas(SRAPATH);
    auto const toolpath = which(tooname);
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
    Dictionary params;
    std::vector<std::string> accessions;
    
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

static void running_as_other_tool [[noreturn]] (int const tool)
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
        running_as_other_tool(tool);
        break;

    default:
        running_as_self();
        break;
    }
    assert(!"reachable");
}

// TODO: factor out and make tests
static bool matched(std::string const &param, int argc, char *argv[], int &i, std::string *value)
{
    auto const len = param.size();
    auto const arg = std::string(argv[i]);
    char const *found = NULL;
    
    if (arg.size() >= len && arg.substr(0, len) == param) {
        if (arg.size() == len) {
            if (i + 1 < argc) {
                found = argv[++i];
            }
        }
        else if (arg[len] == '=') {
            found = argv[i] + len + 1;
        }
    }
    if (!found) return false;

    if (!value->empty())
        std::cerr << "warn: parameter " << param << " was specified more than once, the previous value was overwritten" << std::endl;
    value->assign(found);
    return true;
}

static void main [[noreturn]] (const char *argv0, int argc, char *argv[])
{
    std::string s_selfpath(argv0)
              , s_basename(split_basename(s_selfpath))
              , s_version(split_version(s_basename, "2.10.0"));
    std::string s_location, s_ngc;
    auto s_args = std::vector<std::string>();
    
    s_args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        if (matched("--location", argc, argv, i, &s_location)) {
            location = &s_location;
            continue;
        }
        if (matched("--ngc", argc, argv, i, &s_ngc)) {
            ngc = &s_ngc;
            continue;
        }
        s_args.push_back(std::string(argv[i]));
    }
    selfpath = &s_selfpath;
    basename = &s_basename;
    version_string = &s_version;
    args = &s_args;

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
