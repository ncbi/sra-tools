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

namespace sratools {

std::string const *location = NULL;
std::string const *perm = NULL;
std::string const *ngc = NULL;

Config const *config = NULL;

char const *Accession::extensions[] = { ".sra", ".sralite", ".realign", ".noqual", ".sra" };
char const *Accession::qualityTypes[] = { "Full", "Lite" };
char const *Accession::qualityTypeForFull = qualityTypes[0];
char const *Accession::qualityTypeForLite = qualityTypes[1];

static void test();

static void enableLogging(char const *argv0)
{
    auto const rc = KWrtInit(argv0, TOOLKIT_VERS);
    if (rc == 0)
        KLogLibHandlerSetStdErr();
#ifdef HACKING
    assert(!KDbgSetString("VFS"));
#endif
}

static void handleFileArgument(Argument const &arg, int *const count, char const **const value)
{
    auto const filePath = FilePath(arg.argument);

    ++*count;
    if (!filePath.exists()) {
        arg.reason = Argument::notFound;
        std::cerr << "--" << arg << " not found." << std::endl;
        return;
    }
    if (!filePath.readable()) {
        arg.reason = Argument::unreadable;
        std::cerr << "--" << arg << " not readable." << std::endl;
        return;
    }
    if (*value == nullptr)
        *count = 0;

    if (++*count > 1) {
        LOG(1) << "--" << arg.def->name << " given more than once";
        if (strcmp(*value, arg.argument) == 0) {
            arg.reason = Argument::duplicate;
            --*count;
        }
    }
    else
        *value = arg.argument;
};

static bool checkCommonOptions(Arguments const &args, std::string *gPerm, std::string *gNGC)
{
    auto problems = 0;
    auto permCount = 0;
    auto ngcCount = 0;
    auto cartCount = 0;
    char const *lperm = nullptr;
    char const *lngc = nullptr;
    char const *lcart = nullptr;

    args.each([&](Argument const &arg) {
        if (arg == "perm") {
            handleFileArgument(arg, &permCount, &lperm);
            return;
        }
        if (arg == "ngc") {
            handleFileArgument(arg, &ngcCount, &lngc);
            return;
        }
        if (arg == "cart") {
            handleFileArgument(arg, &cartCount, &lcart);
            return;
        }
    });

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
    if (permCount > 0 && lperm == nullptr) {
        ++problems;
        permCount = 0;
    }
    if (ngcCount > 0 && lngc == nullptr) {
        ++problems;
        ngcCount = 0;
    }
    if (cartCount > 0 && lcart == nullptr) {
        ++problems;
        cartCount = 0;
    }
    if (permCount > 0 && ngcCount > 0) {
        ++problems;
        std::cerr << "--perm and --ngc are mutually exclusive. Please use only one." << std::endl;
    }
    if (permCount > 0 && !vdb::Service::haveCloudProvider()) {
        ++problems;
        std::cerr
            << "Currently, --perm can only be used from inside a cloud computing environment.\n" \
               "Please run inside of a supported cloud computing environment, or get an ngc file" \
               " from dbGaP and reissue the command with --ngc <ngc file> instead of --perm <perm file>."
            << std::endl;
    }
    else if (permCount > 0 && !config->canSendCEToken()) {
        ++problems;
        std::cerr << "--perm requires a cloud instance identity, please run vdb-config and" \
                     " enable the option to report cloud instance identity." << std::endl;
    }

    if (lperm)
        gPerm->assign(lperm);

    if (lngc)
        gNGC->assign(lngc);

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

    config = new Config();
    struct Defer { ~Defer() { delete config; config = nullptr; } } freeConfig;

    std::string auto_perm, auto_ngc, auto_location;
    auto const sessionID = uuid();
    EnvironmentVariables::set(ENV_VAR_SESSION_ID, sessionID);

    test(); ///< needs to be outside of any try/catch; it needs to be able to go BANG!!!

#if DEBUG || _DEBUGGING
    if (argv.toolName == "sratools") {
        if (argv.argc > 1 && std::string(argv.argv[1]) == "help") {
            std::cerr << "usage: sratools print-argbits | help" << std::endl;
            exit(EXIT_SUCCESS);
        }
        if (argv.argc > 1 && std::string(argv.argv[1]) == "print-argbits") {
            printParameterBitmasks(std::cout);
            exit(EXIT_SUCCESS);
        }
        std::cerr << "usage: sratools print-argbits | help" << std::endl;
        exit(EX_USAGE);
    }
#endif

    // MARK: Check for special tools
    if (argv.toolName == "prefetch" || argv.toolName == "srapath")
        Process::reexec(argv);

    try {
        auto const parsed = argumentsParsed(argv);
        if (parsed.countOfCommandArguments() == 0)
            Process::reexec(argv);

        // MARK: Get and set verbosity.
        auto const verbosity = parsed.countMatching("verbose");
        KStsLevelSet(verbosity);

        parsed.each("log-level", [&](Argument const &arg) {
            setLogLevel(arg.argument);
        });

        // MARK: Set debug flags
        parsed.each("debug", [](Argument const &arg) {
            KDbgSetString(arg.argument);
        });

        // MARK: Check for special parameters that trigger immediate tool execution.
        if (parsed.any("help"))
            Process::execHelp(argv);

        if (parsed.any("version"))
            Process::execVersion(argv);

        // MARK: Validate parameter arguments that will get passed to SDL.
        if (!checkCommonOptions(parsed, &auto_perm, &auto_ngc))
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
        auto const all_sources = data_sources::preload(argv, parsed);

        perm = ngc = location = nullptr;
        parsed.each("perm", [](Argument const &arg) { arg.reason = "used"; });
        parsed.each("ngc", [](Argument const &arg) { arg.reason = "used"; });
        parsed.each("cart", [](Argument const &arg) { arg.reason = "used"; });
        parsed.each("location", [](Argument const &arg) { arg.reason = "used"; });

        all_sources.set_ce_token_env_var();

        // TODO: UNCOMMENT WHEN READY; SEE JIRA VDB-5001
        // all_sources.set_param_bits_env_var(parsed.argsUsed());

        for (auto const &arg : parsed) {
            if (!arg.isArgument()) continue;

            auto const &acc = arg.argument;
            auto const sources = all_sources[acc];
            auto success = false;

            for (auto src : sources) {
                if (verbosity > 0 && src.haveQualityType()) {
                    auto const name = src.haveFullQuality() ? fullQualityName : zeroQualityName;
                    auto const desc = src.haveFullQuality() ? fullQualityDesc : zeroQualityDesc;
                    std::cerr << acc << " is an SRA " << name << " file with " << desc << ".\n";
                }
                // MARK: Run the driven tool
                auto const result = Process::runTool(argv, parsed.keep(acc), src.environment);
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
    }
    catch (std::exception const &e) {
        std::cerr << "An error occured: " << e.what() << std::endl << error_continues_message << std::endl;
        return EX_TEMPFAIL;
    }
    catch (...) {
        std::cerr << "An unexpected error occured." << std::endl << error_continues_message << std::endl;
        return EX_TEMPFAIL;
    }

    exit(EXIT_FAILURE);
}

Accession::Accession(std::string const &value)
: value(value)
, alphas(0)
, digits(0)
, vers(0)
, verslen(0)
, ext(0)
, valid(false)
{
    static auto constexpr min_alpha = 3;
    static auto constexpr max_alpha = 6; // WGS can go to 6 now
    static auto constexpr min_digit = 6;
    static auto constexpr max_digit = 9;

    auto const size = value.size();

    while (alphas < size) {
        auto const ch = value[alphas];

        if (!isalpha(ch))
            break;

        ++alphas;
        if (alphas > max_alpha)
            return;
    }
    if (alphas < min_alpha)
        return;

    while (digits + alphas < size) {
        auto const ch = value[digits + alphas];

        if (!isdigit(ch))
            break;

        ++digits;
        if (digits > max_digit)
            return;
    }
    if (digits < min_digit)
        return;

    auto i = digits + alphas;
    if (i == size || value[i] == '.')
        valid = true;
    else
        return;

    for ( ; i < size; ++i) {
        auto const ch = value[i];

        if (verslen > 0 && vers == ext + 1) { // in version
            if (ch == '.')
                ext = i; // done with version
            else if (isdigit(ch))
                ++verslen;
            else
                vers = 0;
            continue;
        }
        if (ch == '.') {
            ext = i;
            continue;
        }
        if (i == digits + alphas + 1 && isdigit(ch)) { // this is the first version digit.
            vers = i;
            verslen = 1;
        }
    }
}

enum AccessionType Accession::type() const {
    if (valid)
        switch (toupper(value[0])) {
        case 'D':
        case 'E':
        case 'S':
            if (toupper(value[1]) == 'R')
                switch (toupper(value[2])) {
                case 'A': return submitter;
                case 'P': return project;
                case 'R': return run;
                case 'S': return study;
                case 'X': return experiment;
                }
        }
    return unknown;
}

std::vector<std::pair<unsigned, unsigned>> Accession::sraExtensions() const
{
    auto result = allExtensions();
    auto i = result.begin();

    while (i != result.end()) {
        auto const &ext = value.substr(i->first, i->second);
        auto found = false;

        for (auto j = 0; j < 4; ++j) {
            if (ext != extensions[j])
                continue;

            *i = {unsigned(j), i->first};
            found = true;
            break;
        }
        if (found)
            ++i;
        else
            i = result.erase(i);
    }

    return result;
}

std::vector<std::pair<unsigned, unsigned>> Accession::allExtensions() const
{
    std::vector<std::pair<unsigned, unsigned>> result;

    for (auto i = digits + alphas; i < value.size(); ++i) {
        if (vers <= i + 1 && i + 1 < vers + verslen)
            continue;
        if (value[i] != '.')
            continue;
        result.push_back({i, 1});
    }
    for (auto & i : result) {
        while (i.first + i.second < value.size() && value[i.first + i.second] != '.')
            ++i.second;
    }
    return result;
}

#if DEBUG || _DEBUGGING
static AccessionType accessionType(std::string const &accession)
{
    return Accession(accession).type();
}

static void testAccessionType() {
    // asserts because these are all hard-coded values
    assert(accessionType("SRR000000") == run);
    assert(accessionType("ERR000000") == run);
    assert(accessionType("DRR000000") == run);
    assert(accessionType("srr000000") == run);

    assert(accessionType("SRA000000") == submitter);
    assert(accessionType("SRP000000") == project);
    assert(accessionType("SRS000000") == study);
    assert(accessionType("SRX000000") == experiment);

    assert(accessionType("SRR000000.2") == run); // not certain of this one

    assert(accessionType("SRR00000") == unknown); // too short
    assert(accessionType("SRF000000") == unknown); // bad type
    assert(accessionType("ZRR000000") == unknown); // bad issuer
    assert(accessionType("SRRR00000") == unknown); // not digits
}
#endif

/**
 * @brief Runs internal tests
 *
 * Does nothing if the environment variable is not set.
 * Does not return if the environment variable is set (but the tests can throw).
 */
static void test() {
    if (logging_state::testing_level() == 1) {
#if (DEBUG || _DEBUGGING) && !WINDOWS
        testAccessionType();
        uuid_test();
#endif
        exit(0);
    }
}

} // namespace sratools

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
