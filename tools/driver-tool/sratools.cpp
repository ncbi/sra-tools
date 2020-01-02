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
#include <unistd.h>
#include <sysexits.h>

#include "sratools2.hpp"
#include "globals.hpp"
#include "constants.hpp"
#include "args-decl.hpp"
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

namespace sratools {

    std::string const *location = NULL;
    std::string const *perm = NULL;
    std::string const *ngc = NULL;

    Config const *config = NULL;

    static void printInstallMessage [[noreturn]] (void);

#if DEBUG || _DEBUGGING
    static void testAccessionType() {
        assert(accessionType("SRR000000") == run);
        assert(accessionType("ERR000000") == run);
        assert(accessionType("DRR000000") == run);
        assert(accessionType("srr000000") == run);

        assert(accessionType("SRA000000") == submitter);
        assert(accessionType("SRP000000") == project);
        assert(accessionType("SRS000000") == study);
        assert(accessionType("SRX000000") == experiment);

        assert(accessionType("SRR000000.2") == run);

        assert(accessionType("SRR00000") == unknown); // too short
        assert(accessionType("SRF000000") == unknown); // bad type
        assert(accessionType("ZRR000000") == unknown); // bad issuer
        assert(accessionType("SRRR00000") == unknown); // not digits
    }
#endif

    static void test() {
#if DEBUG || _DEBUGGING
        auto const envar = getenv("SRATOOLS_TESTING");
        if (envar && std::atoi(envar)) {
            testAccessionType();
            data_sources::test();
            exit(0);
        }
#endif
    }

    int main(int argc, char *argv[], char *envp[], ToolPath const &toolpath)
    {
        LOG(7) << "executable path: " << toolpath.fullpath() << std::endl;

        static auto const error_continues_message = "If this continues to happen, please contact the SRA Toolkit at https://trace.ncbi.nlm.nih.gov/Traces/sra/";

        test();

        int result = -1;

        auto const sessionID = uuid();
        setenv(ENV_VAR_SESSION_ID, sessionID.c_str(), 1);

        config = new Config(toolpath);
        if (config->noInstallID()) {
            printInstallMessage();
        }
        try {
            result = sratools2::main2( argc, argv, toolpath );
        }
        catch (std::exception const &e) {
            std::cerr << "An error occured: " << e.what() << std::endl << error_continues_message << std::endl;
            result = 3;
        }
        catch (...) {
            std::cerr << "An unexpected error occured." << std::endl << error_continues_message << std::endl;
            result = 3;
        }
        delete config;
        config = nullptr;
        return result;
    }

    ToolPath::ToolPath(std::string const &argv0, char *extra[])
    {
        {
            auto const fullpath = get_exec_path(argv0, extra);
            auto const sep = fullpath.find_last_of('/');
            path_ = (sep == std::string::npos) ? "." : fullpath.substr(0, sep);
        }
        {
            auto const sep = argv0.find_last_of('/');
            basename_ = (sep == std::string::npos) ? argv0 : argv0.substr(sep + 1);
        }
        {
            auto const sep = basename_.find_first_of('.');
            if (sep == std::string::npos) {
                version_ = toolkit_version();
            }
            else {
                version_ = basename_.substr(sep + 1);
                basename_.resize(sep);
            }
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
#endif
        return argv0;
    }

    ToolPath makeToolPath(char const *argv0, char *extra[]) {
        return ToolPath(argv0, extra);
    }

    AccessionType accessionType(std::string const &accession)
    {
        auto result = AccessionType::unknown;
        int st = 0;
        int digits = 0;

        for (auto & ch : accession) {
            auto const CH = toupper(ch);
            switch (st) {
            case 0:
                if (!(CH == 'D' || CH == 'E' || CH == 'S')) return unknown;
                ++st;
                break;
            case 1:
                if (CH != 'R') return unknown;
                ++st;
                break;
            case 2:
                switch (CH) {
                case 'A': result = submitter; break;
                case 'P': result = project; break;
                case 'R': result = run; break;
                case 'S': result = study; break;
                case 'X': result = experiment; break;
                default:
                    return unknown;
                }
                ++st;
                break;
            default:
                if (isdigit(ch)) {
                    ++digits;
                    break;
                }
                if (ch != '.') return unknown;
                return digits < 6 ? unknown : result;
            }
        }
        return digits < 6 ? unknown : result;
    }

} // namespace sratools

#include <klib/debug.h> /* KDbgSetString */
#include <klib/log.h> /* KLogLibHandlerSetStdErr */

#if MAC
int main(int argc, char *argv[], char *envp[], char *apple[])
#elif LINUX
int main(int argc, char *argv[], char *envp[])
#else
int main(int argc, char *argv[])
#endif
{
    auto const impersonate = getenv( "SRATOOLS_IMPERSONATE" );
    auto const argv0 = (impersonate && impersonate[0]) ? impersonate : argv[0];

    rc_t rc = KWrtInit(argv[0], 0);
    if (rc == 0)
        rc = KLogLibHandlerSetStdErr();
#ifdef HACKING
    assert(!KDbgSetString("VFS"));
#endif

#if MAC
    return sratools::main(argc, argv, envp, sratools::makeToolPath(argv0, apple));
#elif LINUX
    return sratools::main(argc, argv, envp, sratools::makeToolPath(argv0, nullptr));
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
