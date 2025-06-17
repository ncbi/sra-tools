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
 *  Loader QA Stats
 *
 * Purpose:
 *  Parse command line parameters.
 */

#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <initializer_list>

struct Arguments : public std::vector<std::string> {
    struct Definition {
        char const *name;
        char const *aka;
        char const *defaultValue;
        bool takesParameter;
        int nextIsArg() const { return takesParameter ? 2 : defaultValue != nullptr ? 1 : 0; }
    };
    std::vector< std::pair< std::string, std::optional< std::string > > > parameters;
    std::string program;

    static std::pair<std::string, char const *> split(char const *arg) {
        auto str = std::string(arg);
        char const *value = nullptr;
        auto at = str.find('=');
        if (at != std::string::npos) {
            str = str.substr(0, at);
            value = arg + at + 1;
        }
        return {std::move(str), value};
    }
    Arguments(int argc, char *argv[], std::initializer_list<Definition> &&pdefs) {
        std::vector<Definition> defs{std::move(pdefs)};
        std::vector<std::pair<std::string, char const *>> args;
        defs.emplace_back(Definition{"help", "h?"});
        defs.emplace_back(Definition{"version", "V"});
        {
            program = argv[0];
            auto last = program.size();
            for (auto &ch : program) {
                if (ch == '/')
                    last = (&ch - &program[0]) + 1;
            }
            if (last < program.size())
                program = program.substr(last);
        }
        {
            std::vector<char const *> temp(argv + 1, argv + argc);
            int nextIsArg = 0;

            for (auto arg : temp) {
                if (nextIsArg > 1 || (nextIsArg > 0 && arg[0] != '-')) {
                    nextIsArg = 0;
                    args.back().second = arg;
                    continue;
                }
                if (arg[0] != '-' || arg[1] == '\0') {
                    push_back(std::string(arg));
                    continue;
                }
                if (arg[1] == '-') {
                    auto const &[param, argument] = split(arg + 2);
                    args.push_back({std::move(param), argument});
                    if (argument)
                        continue;
                    for (auto const &def : defs) {
                        if (param == def.name) {
                            nextIsArg = def.nextIsArg();
                            args.back().second = def.defaultValue;
                            break;
                        }
                    }
                    continue;
                }
                bool first = true;
                for (++arg; ; ) {
                    auto const ch = *arg++;
                    if (ch == '\0')
                        break;
                    if (!first && ch == '=') {
                        if (*arg != '\0')
                            args.back().second = arg;
                        break;
                    }
                    if (nextIsArg) {
                        args.back().second = arg;
                        nextIsArg = false;
                        break;
                    }
                    args.push_back({std::string(1, ch), nullptr});
                    first = false;
                    for (auto const &def : defs) {
                        if (def.aka == nullptr)
                            continue;
                        for (auto cp = def.aka; *cp; ++cp) {
                            if (ch == *cp) {
                                nextIsArg = def.nextIsArg();
                                args.back().first.assign(def.name);
                                args.back().second = def.defaultValue;
                                break;
                            }
                        }
                        if (args.back().first == def.name)
                            break;
                    }
                }
            }
        }
        for (auto const &[param, value] : args) {
            decltype(parameters)::value_type newvalue{param, {}};
            if (value)
                newvalue.second = std::string(value);
            parameters.emplace_back(std::move(newvalue));
        }
    }
};

