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
 *  argv manipulations
 *
 */

#include <cctype>
#include <iostream>
#include <fstream>

#include <sysexits.h>

#include "parse_args.hpp"

#include "globals.hpp"

namespace sratools {

static bool string_hasPrefix(std::string const &prefix, std::string const &target)
{
    if (target.size() < prefix.size()) return false;
    
    auto i = prefix.begin();
    auto j = target.begin();
    
    while (i != prefix.end()) {
        if (*i != *j) return false;
        ++i;
        ++j;
    }
    return true;
}

std::tuple<bool, std::string, ArgsList::iterator> matched(std::string const &param, ArgsList::iterator i, ArgsList::iterator end)
{
    auto const len = param.size();
    auto const &arg = *i;
    auto value = std::string();
    auto next = std::next(i);
    
    if (!string_hasPrefix(param, *i))
        goto NOT_FOUND;

    if (len == arg.size()) {
        if (next == end)
            goto NOT_FOUND;
        value = *next++;
    }
    else {
        if (arg[len] != '=')
            goto NOT_FOUND;
        value = i->substr(len + 1);
    }
    return std::make_tuple(true, std::move(value), std::move(next));

NOT_FOUND:
    return std::make_tuple(false, std::move(value), std::move(i));
}

static inline int unescape_char(int ch)
{
    switch (ch) {
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case '\\':
    case '\'':
    case '\"':
        return ch;
    default:
        return -1;
    }
}

static ArgsList argsListFromFile(std::string const &filename)
{
    auto result = ArgsList();
    auto ifs = std::ifstream();
    auto quote = 0;
    auto ws = true;
    auto esc = false;
    auto token = std::string();

    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit); ///< enable throwing
    ifs.open(filename); ///< sets fail, can throw
    ifs.exceptions(std::ifstream::badbit); ///< disable throwing on fail (since get() sets fail on eof!)
    for ( ; ; ) {
        auto const ch = ifs.get(); ///< only throws if bad
        if (ifs.eof()) break;
        
        if (esc) {
            auto const tr = unescape_char(ch);
            
            esc = false;
            if (tr > 0)
                token.append(1, char(tr));
            else {
                std::cerr << "warn: unrecognized escape sequence \"\\" << char(ch) << "\"" << std::endl;
                token.append(1, '\\');
                token.append(1, char(ch));
            }
            continue;
        }
        if (ch == '\\') {
            esc = true;
            continue;
        }

        // quote processing happens after escape processing so you can escape quote chars
        if (quote) {
            if (ch == quote)
                quote = 0;
            else
                token.append(1, char(ch));
            continue;
        }
        if (ch == '\'' || ch == '\"') {
            quote = ch;
            continue;
        }

        // whitespace processing happens after quote processing
        if (std::isspace(ch)) {
            if (!ws) {
                result.push_back(token);
                token.erase();
                ws = true;
            }
            continue;
        }
        ws = false;

        // not escaped, not quoted, and not whitespace
        token.append(1, char(ch));
    }
    if (esc == false && quote == 0) {
        if (!token.empty())
            result.push_back(token);
        return result;
    }
    auto const reason = esc ? "escape sequence" : "quoted string";
    throw std::domain_error(std::string("end of file in ") + reason + ", file: " + filename);
}

auto const optionFileParam = std::string("--option-file");

/// @brief load an args list from a file
///
/// @param filename to load
///
/// @returns the args. Throws on I/O error or parse error.
static ArgsList loadOptionFile(std::string const &filename)
{
    auto result = ArgsList();
    auto file = argsListFromFile(filename);
    auto optionfile = std::string();

    for (auto i = file.begin(); i != file.end(); ) {
        bool found;
        std::string value;
        decltype(i) next;
        
        std::tie(found, value, next) = matched(optionFileParam, i, file.end());
        if (found) {
            auto const &option = loadOptionFile(value);
            std::copy(option.begin(), option.end(), std::back_inserter(result));
            i = next;
            continue;
        }
        result.push_back(*i++);
    }
    return result;
}

ArgsList loadArgv(int argc, char *argv[])
{
    auto result = ArgsList();

    result.reserve(argc);
    // convert argv while expanding any --option-file
    for (int i = 0; i < argc; ) {
        auto const arg = std::string(argv[i++]);

        if (string_hasPrefix(optionFileParam, arg)) {
            auto optionfile = std::string();
            auto opt_eq_file = false;
            
            if (arg.size() == optionFileParam.size()) {
                if (i < argc) {
                    optionfile.assign(argv[i++]);
LOAD_OPTION_FILE:
                    try {
                        auto const &option = loadOptionFile(optionfile);
                        std::copy(option.begin(), option.end(), std::back_inserter(result));
                        continue;
                    }
                    catch (...) {
                        std::cerr << "could not load option file " << optionfile << std::endl;
                        if (opt_eq_file && optionfile[0] == '~')
                            std::cerr << "--option-file=<file> does not do tilde expansion, try using the full path, or $HOME inplace of tilde, or split it into two arguments" << std::endl;
                        exit(EX_NOINPUT);
                    }
                }
                else {
                    std::cerr << "--option-file requires a file name" << std::endl;
                    exit(EX_USAGE);
                }
            }
            else if (arg[optionFileParam.size()] == '=') {
                opt_eq_file = true;
                optionfile.assign(arg.substr(optionFileParam.size() + 1));
                goto LOAD_OPTION_FILE;
            }
        }
        result.push_back(arg);
    }
    return result;
}

bool parseArgs(  ParamList *parameters
               , ArgsList *arguments
               , Dictionary const &longNames
               , Dictionary const &hasArg
               )
{
    auto putback = std::string();
    auto nextIsParamArg = false;
    
    for (auto i = 0; i < args->size() || !putback.empty(); ) {
        auto arg = putback.empty() ? args->at(i++) : putback;
        
        putback.erase();
        
        if (nextIsParamArg) {
            nextIsParamArg = false;
            parameters->back().second = arg;
            continue;
        }
        
        // if it is not an option then it is a regular argument
        if (arg.empty() || arg[0] != '-') {
            arguments->push_back(arg);
            continue;
        }
        
        if (arg[1] == '-') {
            // long form: could be --name | --name=value | or --name value
            auto const eq = arg.find_first_of('=');

            if (eq == std::string::npos) {
                // could be --name | --name value
                parameters->emplace_back(ParamList::value_type(arg, opt_string()));
                nextIsParamArg = hasArg.find(arg) != hasArg.end();
            }
            else {
                // --name=value
                auto const name = arg.substr(0, eq);
                auto const value = arg.substr(eq + 1);
                parameters->emplace_back(ParamList::value_type(name, value));
            }
        }
        else {
            // short form: -abc can mean -a bc | -a -bc
            auto const ch = arg.substr(0, 2); // < std::string not char
            auto const rest = arg.substr(2);
            auto const iter = longNames.find(ch);

            if (iter == longNames.end()) {
                // complain
                std::cerr << "unknown parameter " << ch << std::endl;
                return false;
            }
            auto const &name = iter->second;
            
            if (hasArg.find(name) != hasArg.end()) {
                // -a bc
                if (rest.empty()) {
                    parameters->emplace_back(ParamList::value_type(name, opt_string()));
                    nextIsParamArg = true;
                }
                else
                    parameters->emplace_back(ParamList::value_type(name, rest));
            }
            else {
                // -a -bc
                parameters->emplace_back(ParamList::value_type(name, opt_string()));
                if (!rest.empty())
                    putback = std::string(1, '-') + rest;
            }
        }
    }
    if (nextIsParamArg) return false; // missing argument
    for (auto && arg : *parameters) {
        if (arg.first == "--help") return false;
    }
    return true;
}

} // namespace sratools
