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
#include <functional>

#include <cstdlib>
#if 0 //MAC || LINUX
#include <unistd.h>
#include <sysexits.h>
#endif

#include "sratools2.hpp"
#include "support2.hpp"
#include "globals.hpp"
#include "constants.hpp"
#include "parse_args.hpp"
#include "run-source.hpp"
#include "proc.hpp"
#include "tool-args.hpp"
#include "debug.hpp"
#include "util.hpp"
#include "fastq-dump.hpp"
#include "split_path.hpp"
#include "uuid.hpp"
#include "env_vars.h"
#include "tool-path.hpp"
#include "sratools.hpp"

#include <klib/debug.h> /* KDbgSetString */
#include <klib/log.h> /* KLogLibHandlerSetStdErr */

namespace sratools {

    std::string const *location = NULL;
    std::string const *perm = NULL;
    std::string const *ngc = NULL;

    Config const *config = NULL;

    static void printInstallMessage [[noreturn]] (void);

#if DEBUG || _DEBUGGING
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
#if DEBUG || _DEBUGGING
            testAccessionType();
            uuid_test();
            data_sources::test(); ///< mostly likely to fail due to changes in SDL invalidating the tests
#endif
            exit(0);
        }
    }

    static void enableLogging(char const *argv0)
    {
        auto const rc = KWrtInit(argv0, TOOLKIT_VERS);
        if (rc == 0)
            KLogLibHandlerSetStdErr();
#ifdef HACKING
        assert(!KDbgSetString("VFS"));
#endif
    }

    int main(int argc, char *argv[], char *envp[], ToolPath const &toolpath)
    {
#if DEBUG || _DEBUGGING
        enableLogging(argv[0]); // we probably want to log as ourselves in a debug build
#else
        enableLogging(toolpath.fullpath().c_str());
#endif
        LOG(7) << "executable path: " << toolpath.fullpath() << std::endl;

        static auto const error_continues_message = "If this continues to happen, please contact the SRA Toolkit at https://trace.ncbi.nlm.nih.gov/Traces/sra/";

        test(); ///< needs to be outside of any try/catch; it needs to be able to go BANG!!!

        try {
            auto const sessionID = uuid();
            EnvironmentVariables::set(ENV_VAR_SESSION_ID, sessionID);

            config = new Config(toolpath);
            struct Defer { ~Defer() { delete config; config = nullptr; } } freeConfig;
            if (config->noInstallID()) {
                printInstallMessage();
            }

            auto const &what = sratools2::WhatImposter(toolpath);
            auto const &args = sratools2::Args(argc, argv, EnvironmentVariables::impersonate());
            switch (what._imposter) {
                // normal tools
            case sratools2::Imposter::FASTERQ_DUMP  : return sratools2::impersonate_fasterq_dump(args, what);
            case sratools2::Imposter::FASTQ_DUMP    : return sratools2::impersonate_fastq_dump(args, what);
            case sratools2::Imposter::SAM_DUMP      : return sratools2::impersonate_sam_dump(args, what);
            case sratools2::Imposter::SRA_PILEUP    : return sratools2::impersonate_sra_pileup(args, what);
            case sratools2::Imposter::VDB_DUMP      : return sratools2::impersonate_vdb_dump(args, what);

                // special tools
            case sratools2::Imposter::PREFETCH      : return sratools2::impersonate_prefetch(args, what);
            case sratools2::Imposter::SRAPATH       : return sratools2::impersonate_srapath(args, what);
            default:
                assert(!"reachable");
                abort();
            }
        }
        catch (sratools2::WhatImposter::InvalidToolException const &e) {
            std::cerr << "An error occured: unrecognized tool " << toolpath.basename() << std::endl << error_continues_message << std::endl;
        }
        catch (sratools2::WhatImposter::InvalidVersionException const &e) {
            std::cerr << "An error occured: unrecognized version " << toolpath.version() << ", expected " << toolpath.toolkit_version() << std::endl << error_continues_message << std::endl;
        }
        catch (std::exception const &e) {
            std::cerr << "An error occured: " << e.what() << std::endl << error_continues_message << std::endl;
        }
        catch (...) {
            std::cerr << "An unexpected error occured." << std::endl << error_continues_message << std::endl;
        }
        return EX_TEMPFAIL;
    }

    ToolPath::ToolPath(std::string const &argv0, char *extra[])
    {
        {
            auto const fullpath = get_exec_path(argv0, extra);
            auto const sep = fullpath.find_last_of(ToolPath::seperator);
            path_ = (sep == std::string::npos) ? "." : fullpath.substr(0, sep);
        }
        {
            auto const sep = argv0.find_last_of(ToolPath::seperator);
#if WINDOWS
            if (sep == std::string::npos) {
                auto const sep = argv0.find_first_of(':'); // look for a drive letter
                basename_ = (sep == std::string::npos) ? argv0 : (argv0.substr(sep + 1));
            }
            else
                basename_ = argv0.substr(sep + 1);
#else
            basename_ = (sep == std::string::npos) ? argv0 : argv0.substr(sep + 1);
#endif
        }
        {
#if WINDOWS
            version_ = toolkit_version();
#else
            auto const sep = basename_.find_first_of('.');
            if (sep == std::string::npos) {
                version_ = toolkit_version();
            }
            else {
                version_ = basename_.substr(sep + 1);
                basename_.resize(sep);
            }
#endif
        }
    }

    std::string ToolPath::get_exec_path(std::string const &argv0, char *extra[])
    {
#if MAC
        if (extra) {
            for (auto i = extra; *i; ++i) {
                if (starts_with("executable_path=", *i)) {
                    return *i + 16;
                }
            }
        }
#elif LINUX
        {
            auto const path = realpath("/proc/self/exe", nullptr);
            if (path) {
                auto const &result = std::string(path);
                free(path);
                return result;
            }
        }
#elif WINDOWS
        {
            auto const path = GetFullPathToExe();
            if (path) {
                auto result = std::string(path);
                free(path);
                // remove .exe
                if (ends_with(".exe", result))
                    result = result.substr(0, result.size() - 4);
                return result;
            }
        }
#endif
        return argv0;
    }

    ToolPath makeToolPath(char const *argv0, char *extra[]) {
        return ToolPath(argv0, extra);
    }

    bool isSRAPattern(std::string const &accession)
    {
        // as specified in get_accession_code and get_accession_app in vfs/resolver.c
        // the pattern is 3 alpha followed by 6 to 9 digits
        auto constexpr min_alpha = 3;
        auto constexpr max_alpha = 3;
        auto constexpr min_digit = 6;
        auto constexpr max_digit = 9;
        auto const size = accession.size();
        auto alphas = decltype(size)(0);
        auto digits = decltype(size)(0);

        while (alphas < size) {
            auto const ch = accession[alphas];

            if (!isalpha(ch))
                break;

            ++alphas;
            if (alphas > max_alpha)
                return false; ///< too many alpha characters
        }
        assert(alphas <= max_alpha);
        if (alphas < min_alpha)
            return false; /// < too few alpha characters (or too few characters)

        while (digits + alphas < size) {
            auto const ch = accession[digits + alphas];

            if (!isdigit(ch))
                break;

            ++digits;
            if (digits > max_digit)
                return false; ///< too many digit characters
        }
        assert(digits <= max_digit);
        if (digits < min_digit)
            return false; ///< too few digit characters

        if (digits + alphas == size)
            return true;

        assert(digits + alphas < size);
        if (accession[digits + alphas] != '.')
            return false; ///< extraneous characters

        auto version = 0;
        for (auto i = digits + alphas + 1; i < size; ++i) {
            if (!isdigit(accession[i]))
                return false; ///< extraneous characters
            ++version;
        }
        return (version > 0 && (digits + alphas + 1 + version) == size);
    }

    AccessionType accessionType(std::string const &accession)
    {
        if (!isSRAPattern(accession))
            return unknown;

        auto const issuer = toupper(accession[0]);
        auto const read = toupper(accession[1]);
        auto const type = toupper(accession[2]);

        switch (issuer) {
        case 'D':
        case 'E':
        case 'S':
            break;
        default:
            return unknown;
        }

        if (read != 'R')
            return unknown;

        switch (type) {
        case 'A': return submitter;
        case 'P': return project;
        case 'R': return run;
        case 'S': return study;
        case 'X': return experiment;
        default:  return unknown;
        }
    }

} // namespace sratools

#if WINDOWS
/// compute the number of UTF8 bytes needed to store an array of Windows wchar strings
/// Note: the count includes null terminators
static size_t needUTF8s(int argc, wchar_t *wargv[])
{
    size_t result = 0;
    for (int i = 0; i < argc; ++i) {
        auto const count = Win32Shim::unwideSize(wargv[i]);
        if (count <= 0)
            throw std::runtime_error("Can not convert command line to UTF-8!?"); ///< Windows shouldn't ever send us strings that it can't convert to UTF8
        result += count;
    }
    return result;
}

/// convert an array of Windows wchar strings into an array of UTF8 strings
static void convert2UTF8(int argc, char *argv[], size_t bufsize, char *buffer, wchar_t *wargv[])
{
    int i;
    for (i = 0; i < argc; ++i) {
        auto const count = Win32Shim::unwiden(buffer, bufsize, wargv[i]);
        assert(0 < count && (size_t)count <= bufsize); ///< should never be < 0, since we should have caught that in `needUTF8s`
        argv[i] = buffer;
        buffer += count;
        bufsize -= count;
    }
    argv[i] = NULL;
}

/// convert an array of Windows wchar strings into an array of UTF8 strings
/// returns a new array that can be `free`d
/// Note: don't free the individual strings in the array
static char **convertWStrings(int argc, wchar_t *wargv[])
{
    auto const chars = needUTF8s(argc, wargv);
    auto const argv = (char **)malloc((argc + 1) * sizeof(char *) + chars);
    auto const buffer = (char *)&argv[argc + 1];

    if (argv != NULL)
        convert2UTF8(argc, argv, chars, buffer, wargv);

    return argv;
}
#endif

#if MAC
int main(int argc, char *argv[], char *envp[], char *apple[])
#elif LINUX
int main(int argc, char *argv[], char *envp[])
#elif WINDOWS

int wmain(int argc, wchar_t *wargv[], wchar_t *wenvp[])
#else
int main(int argc, char *argv[])
#endif
{
#if WINDOWS
    auto const up_argv = std::unique_ptr<char *, decltype(deleterFree)>(convertWStrings(argc, wargv), deleterFree);
    auto const argv = up_argv.get();
#endif
    auto const impersonate = EnvironmentVariables::impersonate();
    auto const argv0 = (impersonate && impersonate[0]) ? impersonate : argv[0];

#if MAC
    return sratools::main(argc, argv, envp, sratools::makeToolPath(argv0, apple));
#elif LINUX
    return sratools::main(argc, argv, envp, sratools::makeToolPath(argv0, nullptr));
#elif WINDOWS
    return sratools::main(argc, argv, nullptr, sratools::makeToolPath(argv0, nullptr));
#else
    return sratools::main(argc, argv, nullptr, sratools::makeToolPath(argv0, nullptr));
#endif
}
#endif // c++11

namespace sratools {
    static void printInstallMessage [[noreturn]] (void)
    {
        std::cerr
        <<
        "This sra toolkit installation has not been configured.\n"
        "Before continuing, please run: vdb-config --interactive\n"
        "For more information, see https://www.ncbi.nlm.nih.gov/sra/docs/sra-cloud/"
        << std::endl;

        exit(EX_CONFIG);
    }
}
