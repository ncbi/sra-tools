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

// C main is at the end of the file

#if __cplusplus < 201103L && 0
#error c++11 or higher is needed
#else

#include "util.hpp"

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
#include <functional>

#include <cstdlib>
#include <cstring>

#include "globals.hpp"
#include "constants.hpp"
#include "debug.hpp"
#include "sratools.hpp"
#include "command-line.hpp"
#include "uuid.hpp"
#include "run-source.hpp"
#include "SDL-response.hpp"
#include "service.hpp"
#include "proc.hpp"
#include "../../shared/toolkit.vers.h"

#include <klib/debug.h> /* KDbgSetString */
#include <klib/log.h> /* KLogLibHandlerSetStdErr */
#include <klib/status.h> /* KStsLevelSet */

namespace sratools {

std::string const *location = NULL;
FilePath const *perm = NULL;
FilePath const *ngc = NULL;

Config const *config = NULL;

static void enableLogging(char const *argv0)
{
    auto const rc = KWrtInit(argv0, TOOLKIT_VERS);
    if (rc == 0)
        KLogLibHandlerSetStdErr();
#ifdef HACKING
    assert(!KDbgSetString("VFS"));
#endif
}

static void handleFileArgument(Argument const &arg, FilePath const &filePath, std::set< std::string > *param_args, int *count)
{
    if (param_args->insert(std::string(arg.argument)).second) {
        if (!filePath.exists())
            arg.reason = Argument::notFound;
        else if (!filePath.readable())
            arg.reason = Argument::unreadable;
        else
            ++*count;
    }
    else
        arg.reason = Argument::duplicate;
};

static unsigned handleFileArgumentErrors(char const *const argName, Arguments const &args)
{
    unsigned problems = 0;

    args.each(argName, [&](Argument const &arg) {
        if (arg.ignore()) {
            auto const reason = arg.reason;

            if (logging_state::is_dry_run()) // allow bad argument for testing
                arg.reason = nullptr;

            if (reason == Argument::notFound) {
                ++problems;
                std::cerr << "--" << argName << " " << arg.argument << "\nFile not found." << std::endl;
                return;
            }
            if (reason == Argument::unreadable) {
                ++problems;
                std::cerr << "--" << argName << " " << arg.argument << " permission denied." << std::endl;
                return;
            }
            if (reason == Argument::duplicate) {
                LOG(1) << "--" << argName << " " << arg.argument << " duplicate parameter." << std::endl;
                return;
            }
        }
    });
    return problems;
}

static unsigned checkForContainers(CommandLine const &argv, Arguments const &args)
{
    unsigned result = 0;

    args.eachArgument([&](Argument const &arg) {
        auto const &dir_base = argv.pathForArgument(arg).split();
        if (dir_base.first) // ignore if it looks like a path
            return;

        auto const &accession = Accession(dir_base.second);

        switch (accession.type()) {
        case unknown:
            LOG(3) << arg.argument << " doesn't look like an SRA accession." << std::endl;
            break;
        case run:
            break;
        default:
            std::cerr << arg.argument << " is not a run accession. For more information, see https://www.ncbi.nlm.nih.gov/sra/?term=" << arg.argument << std::endl;
            ++result;
            break;
        }
    });
    return result;
}

static bool checkCommonOptions(CommandLine const &argv, Arguments const &args, FilePath *sPerm, FilePath *sNGC)
{
    int problems = 0;
    int permCount = 0;
    int ngcCount = 0;
    int cartCount = 0;
    auto havePerm = false;
    auto haveNGC = false;
    auto haveCart = false;
    std::set<std::string> param_args;

    args.each([&](Argument const &arg) {
        if (arg == "perm")
            handleFileArgument(arg, argv.pathForArgument(arg), &param_args, &permCount);
    });
    havePerm = !param_args.empty();

    param_args.clear();
    args.each([&](Argument const &arg) {
        if (arg == "ngc")
            handleFileArgument(arg, argv.pathForArgument(arg), &param_args, &ngcCount);
    });
    haveNGC = !param_args.empty();

    param_args.clear();
    args.each([&](Argument const &arg) {
        if (arg == "cart")
            handleFileArgument(arg, argv.pathForArgument(arg), &param_args, &cartCount);
    });
    haveCart = !param_args.empty();

    if (permCount > 1) {
        ++problems;
        std::cerr << "Only one --perm file can be used at a time." << std::endl;
    }
    if (ngcCount > 1) {
        ++problems;
        std::cerr << "Only one --ngc file can be used at a time." << std::endl;
    }
    if (cartCount > 1) {
        ++problems;
        std::cerr << "Only one --cart file can be used at a time." << std::endl;
    }

    if (havePerm) {
        if (haveNGC) {
            ++problems;
            std::cerr << "--perm and --ngc are mutually exclusive. Please use only one." << std::endl;
        }
        problems += handleFileArgumentErrors("perm", args);
        if (!vdb::Service::haveCloudProvider()) {
            ++problems;
            std::cerr
                << "Currently, --perm can only be used from inside a cloud computing environment.\n" \
                   "Please run inside of a supported cloud computing environment, or get an ngc file" \
                   " from dbGaP and reissue the command with --ngc <ngc file> instead of --perm <perm file>."
                << std::endl;
        }
        else if (!config->canSendCEToken()) {
            ++problems;
            std::cerr << "--perm requires a cloud instance identity, please run vdb-config and" \
                         " enable the option to report cloud instance identity." << std::endl;
        }
        else {
            args.each("perm", [&](Argument const &arg) {
                *sPerm = argv.pathForArgument(arg);
            });
        }
    }
    if (haveNGC) {
        unsigned const moreProblems = handleFileArgumentErrors("ngc", args);
        if (moreProblems)
            problems += moreProblems;
        else {
            args.each("ngc", [&](Argument const &arg) {
                *sNGC = argv.pathForArgument(arg);
            });
        }
    }
    if (haveCart) {
        problems += handleFileArgumentErrors("cart", args);
    }
    auto const containers = checkForContainers(argv, args);
    if (containers > 0) {
        std::cerr << "Automatic expansion of container accessions is not currently available. " \
                     "See the above link(s) for information about the accessions." << std::endl;
        problems += containers;
    }

    if (problems == 0)
        return true;

    if (logging_state::is_dry_run()) {
        std::cerr << "Problems allowed for testing purposes!" << std::endl;
        return true;
    }
    return false;
}

/// \brief Identifiers are ([A-Za-z_][A-Za-z0-9_]*)
static bool isCharacterClassIdentifierFirst(char const ch)
{
    return ch == '_' || isalpha(ch);
}

/// \brief Identifiers are ([A-Za-z_][A-Za-z0-9_]*)
static bool isCharacterClassIdentifier(char const ch)
{
    return ch == '_' || isalnum(ch);
}

/// \brief Identifiers are ([A-Za-z_][A-Za-z0-9_]*)
static bool containsIdentifier(char const *whole, char const *const query)
{
    assert(whole != nullptr && query != nullptr);

    auto canStartMatching = true;
    for ( ; ; ++whole) {
        auto const ch = *whole;

        if (ch == '\0')
            return false;

        if (canStartMatching && ch == *query) {
            auto w = whole + 1;
            auto q = query + 1;

            for ( ; *w != '\0' && *q != '\0' && *w == *q; ++w, ++q)
                ;
            if (*q == '\0' && !isCharacterClassIdentifier(*w))
                return true;
            canStartMatching = false;
        }
        else
            canStartMatching = !isCharacterClassIdentifierFirst(ch);
    }
}

static void setLogLevel(char const *logLevelString)
{
    auto const logLevelStrings = KLogGetParamStrings();
    int level = klogLevelMax + 1;

    if (isdigit(*logLevelString)) {
        while (isdigit(*logLevelString))
            level = (level * 10) + ((*logLevelString++) - '0');
    }
    else if (*logLevelString == '-' || *logLevelString == '+') {
        level = klogLevelMin;
        for ( ; ; ) {
            auto const ch = *logLevelString++;

            if (ch == '-')
                --level;
            else if (ch == '+')
                ++level;
            else
                break;
        }
    }
    else {
        for (int i = klogLevelMin; i <= klogLevelMax; ++i) {
            auto const n = strlen(logLevelStrings[i]);
            if (std::strncmp(logLevelString, logLevelStrings[i], n) == 0) {
                logLevelString += n;
                level = i;
                break;
            }
        }
    }
    if (*logLevelString == '\0')
        KLogLevelSet(level);
}

static bool vdbDumpPrefersLiteFormat(Arguments const &args)
{
    // Is vdb-dump skipping QUALITY column?

    // if there is a column list and it does NOT contain QUALITY column
    int qual = 0, col = 0;
    args.each("columns", [&](Argument const &arg) {
        ++col;
        if (containsIdentifier(arg.argument, "QUALITY"))
            ++qual;
    });
    if (col > 0 && qual == 0)
        return true;

    // if there is a column exclude list and it does contain QUALITY column
    return args.firstWhere("exclude", [](Argument const &arg) {
        return containsIdentifier(arg.argument, "QUALITY");
    });
}

static bool fasterqDumpPrefersLiteFormat(Arguments const &args)
{
    /// Is fasterq-dump outputting FASTA?
    return args.any("fasta") || args.any("fasta-unsorted");
}

static bool fastqDumpPrefersLiteFormat(Arguments const &args)
{
    /// Is fastq-dump outputting FASTA?
    return args.any("fasta");
}

static bool shouldPreferLiteFormat(bool isSet, bool isFull, std::string const &toolName, Arguments const &args)
{
    if (!isSet || isFull) {
        // MARK: Checks for QUALITY type preference change go here.
        if (toolName == "fasterq-dump")
            return fasterqDumpPrefersLiteFormat(args);

        if (toolName == "fastq-dump")
            return fastqDumpPrefersLiteFormat(args);

        if (toolName == "vdb-dump")
            return vdbDumpPrefersLiteFormat(args);
    }
    return false;
}

static void printHelp [[noreturn]] (bool requested = false)
{
    (requested ? std::cout : std::cerr)
        << "usage: sratools print-argbits | print-args-json | help\n"
        << "Please see https://github.com/ncbi/sra-tools/wiki/sratools-driver-tool for an explanation of the sratools driver tool."
        << std::endl;
    exit(requested ? EXIT_SUCCESS : EX_USAGE);
}

static void do_sratools [[noreturn]] (CommandLine const &argv)
{
    if (argv.argc > 1) {
        auto const arg = std::string(argv.argv[1]);

        if (arg == "help")
            printHelp(true);

        if (arg == "print-argbits") {
            printParameterBitmasks(std::cout);
            exit(EXIT_SUCCESS);
        }

        if (arg == "print-args-json") {
            printParameterJSON(std::cout);
            exit(EXIT_SUCCESS);
        }
    }
    printHelp();
}

static auto constexpr error_continues_message = "If this continues to happen, please contact the SRA Toolkit at https://trace.ncbi.nlm.nih.gov/Traces/sra/";
static auto constexpr fullQualityName = "Normalized Format";
static auto constexpr zeroQualityName = "Lite";
static auto constexpr fullQualityDesc = "full base quality scores";
static auto constexpr zeroQualityDesc = "simplified base quality scores";

static int main(CommandLine const &argv)
{
#if DEBUG || _DEBUGGING
    enableLogging(argv.realName.c_str()); // we probably want to log as ourselves in a debug build
#else
    enableLogging(argv.toolName.c_str());
#endif
    LOG(7) << "executable path: " << (std::string)argv.fullPathToExe << std::endl;
    // std::cerr << "executable path: " << (std::string)argv.fullPathToExe << std::endl;

    config = new Config();
    struct Defer { ~Defer() { delete config; config = nullptr; } } freeConfig;

    FilePath auto_perm, auto_ngc;
    std::string auto_location;
    auto const sessionID = uuid();
    EnvironmentVariables::set(ENV_VAR_SESSION_ID, sessionID);

#if DEBUG || _DEBUGGING
    if (argv.toolName == "sratools")
        do_sratools(argv);
#endif

    if (!logging_state::is_dry_run() && argv.isShortCircuit()) {
        std::cerr << "This installation is damaged. Please reinstall the SRA Toolkit." << std::endl;
        exit(EX_SOFTWARE);
    }

    if (argv.toolName == "srapath") ///< srapath is special
        Process::reexec(argv); ///< noreturn

    try {
        auto const parsed = argumentsParsed(argv);
        if (parsed.countOfCommandArguments() == 0)
            Process::reexec(argv); ///< noreturn

        // MARK: set parameters-used bitfield in environment
        data_sources::set_param_bits_env_var(parsed.argsUsed());

        if (argv.toolName == "prefetch") ///< prefetch is special
            Process::reexec(argv); ///< noreturn

        // MARK: Get and set verbosity.
        auto const verbosity = parsed.countMatching("verbose");
        KStsLevelSet(verbosity);

        // MARK: set log severity filter
        parsed.each("log-level", [](Argument const &arg) {
            setLogLevel(arg.argument);
        });

        // MARK: Set debug flags
#if _DEBUGGING
        parsed.each("debug", [](Argument const &arg) {
            KDbgSetString(arg.argument);
        });
#endif

        // MARK: Check for special parameters that trigger immediate tool execution.
        if (parsed.any("help"))
            Process::execHelp(argv);

        if (parsed.any("version"))
            Process::execVersion(argv);

        // MARK: Validate parameter arguments that will get passed to SDL.
        if (!checkCommonOptions(argv, parsed, &auto_perm, &auto_ngc))
            return EX_USAGE;

        if (!auto_perm.empty())
            perm = &auto_perm;

        if (!auto_ngc.empty())
            ngc = &auto_ngc;

        parsed.first("location", [&](Argument const &arg) {
            auto_location.assign(arg.argument);
            location = &auto_location;
        });

        // MARK: Get QUALITY type preference.
        auto const qualityPreference = data_sources::qualityPreference();
        if (verbosity > 0 && qualityPreference.isSet) {
            auto const name = qualityPreference.isFullQuality ? fullQualityName : zeroQualityName;
            auto const desc = qualityPreference.isFullQuality ? fullQualityDesc : zeroQualityDesc;
            std::cerr << "Preference setting is: Prefer SRA " << name << " files with " << desc << " if available.\n";
        }

        // MARK: Check if tool wants to override QUALITY type preference.
        if (shouldPreferLiteFormat(qualityPreference.isSet, qualityPreference.isFullQuality, argv.toolName, parsed)) {
            if (verbosity > 0 && qualityPreference.isSet && qualityPreference.isFullQuality) {
                std::cerr << argv.toolName << " requests temporary preference change: Prefer SRA " << zeroQualityName << " files with " << zeroQualityDesc << " if available.\n";
            }
            data_sources::preferNoQual();
        }

        // MARK: Look for tool arguments in the file system or ask SDL about them.
        auto const &all_sources = data_sources::preload(argv, parsed);

        perm = ngc = nullptr; // been used and isn't needed any longer
        location = nullptr; // been used and isn't needed any longer
//        parsed.each("perm", [](Argument const &arg) { arg.reason = "used"; });
//        parsed.each("ngc", [](Argument const &arg) { arg.reason = "used"; });
//        parsed.each("cart", [](Argument const &arg) { arg.reason = "used"; });
        parsed.each("location", [](Argument const &arg) { arg.reason = "used"; });

        all_sources.set_ce_token_env_var();

        for (auto const &arg : parsed) {
            if (!arg.isArgument()) continue;

            auto const acc = arg.argument;
            auto const &sources = all_sources[acc];
            auto success = false;

            for (auto src : sources) {
                if (verbosity > 0 && src.haveQualityType()) {
                    auto const name = src.haveFullQuality() ? fullQualityName : zeroQualityName;
                    auto const desc = src.haveFullQuality() ? fullQualityDesc : zeroQualityDesc;
                    std::cerr << acc << " is an SRA " << name << " file with " << desc << ".\n";
                }
                // MARK: Run the driven tool
                auto const result = Process::runTool(argv, parsed.keep(arg), src.environment);
                if (result.didExitNormally()) {
                    success = true;
                    LOG(2) << "Processed " << acc << " with data from " << src.service << std::endl;
                    break;
                }
                if (result.didExit()) {
                    auto const exit_code = result.exitCode();
                    if (exit_code == EX_TEMPFAIL) {
                        LOG(1) << "Failed to get data for " << acc << " from " << src.service << std::endl;
                        continue;
                    }
                    std::cerr << argv.toolName << " quit with error code " << exit_code << std::endl;
                    exit(exit_code);
                }
                // was killed or something
                exit(result.exitCode());
            }
            if (!success) {
                std::cerr << "Could not get any data for " << acc << ", tried to get data from:" << std::endl;
                for (auto i : sources) {
                    std::cerr << '\t' << i.service << std::endl;
                }
                std::cerr << "This may be temporary, retry later." << std::endl;
                return EX_TEMPFAIL;
            }
        }
        return 0;
    }
    catch (UnknownToolException) {
        std::cerr << "An error occured: unrecognized tool " << argv.runAs() << std::endl << error_continues_message << std::endl;
        return EXIT_FAILURE;
    }
    catch (std::exception const &e) {
        std::cerr << "An error occured: " << e.what() << std::endl << error_continues_message << std::endl;
        return EX_TEMPFAIL;
    }
    catch (...) {
        std::cerr << "An unexpected error occured." << std::endl << error_continues_message << std::endl;
        return EX_TEMPFAIL;
    }
}

} // namespace sratools

// BSD is defined when compiling on Mac
// Use the MAC case below, not this one
#if BSD && !MAC
int main(int argc, char *argv[], char *envp[])
{
    auto const invocation = CommandLine(argc, argv, envp, nullptr);
    return sratools::main(invocation);
}
#endif

#if MAC
int main(int argc, char *argv[], char *envp[], char *apple[])
{
    auto const invocation = CommandLine(argc, argv, envp, apple);
    return sratools::main(invocation);
}
#endif

#if LINUX
int main(int argc, char *argv[], char *envp[])
{
    auto const invocation = CommandLine(argc, argv, envp, nullptr);
    return sratools::main(invocation);
}
#endif

#if WINDOWS
#if USE_WIDE_API
int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
    auto const invocation = CommandLine(argc, argv, envp, nullptr);
    return sratools::main(invocation);
}
#else
int main(int argc, char *argv[], char *envp[])
{
    auto const invocation = CommandLine(argc, argv, envp, nullptr);
    return sratools::main(invocation);
}
#endif
#endif

#endif // c++11
