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
*  load config
*
*/

#include <iostream>
#include <cctype>
#include <set>
#include <cstdlib>

#include "support2.hpp"
#include "config.hpp"
#include "proc.hpp"
#include "tool-path.hpp"
#include "util.hpp"
#include "debug.hpp"

namespace sratools {

#if DEBUG || _DEBUGGING
static bool forceInstallID(void)
{
#if WINDOWS
    return false;
#else // POSIX
    auto const envar = getenv("SRATOOLS_FORCE_INSTALL");
    return envar && envar[0] && !(envar[0] == '0' && envar[1] == '\0');
#endif
}
#endif

static bool ignore(std::string const &key)
{
    static std::set<std::string> const keys_to_ignore = {
        { "/APPNAME" },
        { "/APPPATH" },
        { "/BUILD" },
        { "/HOME" },
        { "/NCBI_HOME" },
        { "/NCBI_SETTINGS" },
        { "/PWD" },
        { "/USER" }
    };
    auto const fnd = keys_to_ignore.find(key);
    if (fnd != keys_to_ignore.end())
        return true;
    if (key.size() >= 9 && key.substr(0, 9) == "/VDBCOPY/")
        return true;
    return false;
}

Config::Config(ToolPath const &runpath) {
    static char const *argv[] = {
        "vdb-config", "--output=n",
        NULL
    };
    auto const toolpath = runpath.getPathFor(argv[0]);
    if (toolpath.executable()) {
        auto const path = toolpath.fullpath();
        std::string raw;
        auto const rc = process::run_child_and_get_stdout(&raw, path.c_str(), argv[0], argv, {});
        if (rc.exited() && rc.exit_code() == 0) {
#if DEBUG || _DEBUGGING
            auto haveInstallID = false;
#endif
            auto start = raw.cbegin();
            auto const end = raw.cend();
            auto section = 0;
            auto state = 0;
            auto ws = true; // eat whitespace
            auto key = Container::key_type();
            auto value = std::string();
            auto parse = [&](std::string const &line) {
                for (auto ch : line) {
                    if (ws && isspace(ch)) continue;
                    ws = false;
                    
                    if (state == 5) {
                        if (ch == '"') {
                            ws = true;
                            state = 0;
#if DEBUG || _DEBUGGING
                            if (key == InstallKey())
                                haveInstallID = true;
#endif
                            if (!ignore(key))
                                kvps.emplace(key, value);
                            continue;
                        }
                        if (ch == '\\')
                            state = 6;
                        else
                            value.append(1, ch);
                        continue;
                    }
                    if (state == 2) {
                        if (isspace(ch)) {
                            ws = true;
                            state = 3;
                            continue;
                        }
                        if (ch != '=') {
                            key.append(1, ch);
                            continue;
                        }
                        state = 3;
                        // falls into next
                    }
                    if (state == 3) {
                        if (ch == '=') {
                            ws = true;
                            state = 4;
                            continue;
                        }
                        throw std::domain_error("expected '=' after "+key+" in "+line);
                    }
                    if (state == 0) {
                        if (ch == '#')
                            return; // discard comment line
                        if (ch == '/') {
                            key = "/";
                            state = 1;
                            continue;
                        }
                        throw std::domain_error("expected key in "+line);
                    }
                    if (state == 1) {
                        if (('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z') || ('0' <= ch && ch <= '9') || ch == '_') {
                            key.append(1, ch);
                            state = 2;
                            continue;
                        }
                        throw std::domain_error("invalid character in "+line);
                    }
                    if (state == 4) {
                        if (ch == '"') {
                            value.erase();
                            state = 5;
                            continue;
                        }
                        throw std::domain_error("expected value in "+line);
                    }
                    if (state == 6) {
                        value.append(1, ch);
                        state = 5;
                        continue;
                    }
                }
            };
            
            for (auto i = start; i != end; ) {
                auto const ch = *i++;
                if (ch == '\n' || i == end) {
                    auto const line = std::string(start, i);
                    start = i;
                    
                    switch (section) {
                    case 0:
                        if (line == "<!-- Current configuration -->\n") {
                            section += 1;
                            state = 0;
                            continue;
                        }
                        break;
                    case 1:
                        if (line == "<!-- Configuration files -->\n") {
                            section += 1;
                            continue;
                        }
                        parse(line);
                        break;
                    case 2:
                        if (line == "<!-- Environment -->\n") {
                            section += 1;
                            continue;
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            if (state != 0) {
                std::cerr << "unterminated value in configuration" << std::endl;
                exit(EX_CONFIG);
            }
#if DEBUG || _DEBUGGING
            if (!haveInstallID && forceInstallID())
                kvps.emplace(InstallKey(), "8badf00d-1111-4444-8888-deaddeadbeef");
#endif
            return;
        }
        if (rc.exited() && rc.exit_code() != 0) {
            std::cerr << "vdb-config exited with error code " << rc.exit_code() << std::endl;
            exit(rc.exit_code());
        }
        else if (rc.signaled()) {
            std::cerr << "vdb-config was killed by signal " << rc.termsig() << std::endl;
            if (rc.coredump())
                std::cerr << "a coredump was generated" << std::endl;
            exit(EX_SOFTWARE);
        }
        else {
            std::cerr << "vdb-config stopped unexpectedly" << std::endl;
            exit(EX_NOINPUT);
        }
    }
    std::cerr << "configuration not loaded: no vdb-config found" << std::endl;
}

}
