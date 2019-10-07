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
 *  locate executable
 *
 */

#include <string>
#include <map>
#include <set>
#include <vector>
#include <iostream>

#include <cstdlib>

#include <unistd.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "globals.hpp"
#include "which.hpp"
#include "debug.hpp"

namespace sratools {

static void append_if_usable(std::vector<std::string> &PATH, std::string const &path, std::set<std::string> &unique)
{
    if (path.size() == 0) return;

    auto const rp = realpath(path.c_str(), NULL);
    if (rp == NULL) return;
    
    auto const real = std::string(rp);
    free(rp);
    if (unique.find(real) != unique.end()) return;
    
    struct stat st;
    if (stat(real.c_str(), &st) == 0 && (st.st_mode & S_IFDIR) != 0 && access(real.c_str(), X_OK) == 0) {
        DEBUG_OUT << "PATH: " << real << std::endl;
        
        PATH.push_back(real);
        unique.insert(real);
    }
}

static std::vector<std::string> loadPATH()
{
    auto unique = std::set<std::string>();
    auto result = std::vector<std::string>();
    char const *const PATH = getenv("PATH");

    LOG(9) << "ENV{PATH}=" << PATH << std::endl;
    
    if (PATH == NULL || PATH[0] == '\0')
        return result;
    
    auto begin = PATH; ///< start of current token
    auto end = begin;
    while (*end != '\0') {
        auto const save = end;
        auto const ch = *end++;
        if (ch == ':') {
            append_if_usable(result, std::string(begin, save), unique);
            begin = end;
        }
    }
    append_if_usable(result, std::string(begin), unique);
    
    return result;
}

static std::string is_exe(std::string const &toolname, std::string const &directory)
{
    auto const fullpath = directory + '/' + toolname;

    // TRACE(fullpath);
    LOG(9) << "looking for " << fullpath << std::endl;
    
    return access(fullpath.c_str(), X_OK) == 0 ? fullpath : "";
}

static std::string which_noversion(std::string const &toolname, std::vector<std::string> const &PATH)
{
    auto fullpath = is_exe(toolname, *selfpath);
    if (fullpath.size() == 0) {
        for (auto & dir : PATH) {
            fullpath = is_exe(toolname, dir);
            if (fullpath.size() != 0)
                break;
        }
        if (fullpath.size() == 0)
            fullpath = is_exe(toolname, ".");
    }
    return fullpath;
}

static std::string which_versioned(std::string const &toolname, std::vector<std::string> const &PATH)
{
    auto candidate = std::vector<std::string>();
    auto versioned = toolname + '.' + *version_string;
    auto fullpath = std::string();

    candidate.reserve(4);
    
    for (auto i = versioned.begin(); i != versioned.end(); ++i) {
        if (*i == '.')
            candidate.push_back(std::string(versioned.begin(), i));
    }
    candidate.push_back(versioned);
    
    for (auto i = candidate.rbegin(); i != candidate.rend(); ++i) {
        fullpath = which_noversion(*i, PATH);
        if (fullpath.size() != 0)
            break;
    }
    return fullpath;
}

static void pathHelp [[noreturn]] (std::string const &toolname, bool const isSraTools);

std::string which(std::string const &toolname, bool const allowNotFound, bool const isaSraTool)
{
    static std::map<std::string, std::string> cache;
    static std::vector<std::string> PATH = loadPATH();

    auto const witch = isaSraTool ? which_versioned : which_noversion;
    auto const iter = cache.find(toolname);
    auto const in_cache = iter != cache.end();
    auto const &found = in_cache ? iter->second : witch(toolname, PATH);
    
    if (!in_cache)
        cache.insert({toolname, found}); ///> cache result (even if not found)

    if (found.size() != 0 || allowNotFound)
        return found;

    pathHelp(toolname, isaSraTool);
}

/// @brief: prints help message for failing to find an executable
static void pathHelp [[noreturn]] (std::string const &toolname, bool const isaSraTool)
{
    std::cerr << "could not find " << toolname;
    if (isaSraTool)
        std::cerr << " which is part of this software distribution";
    std::cerr << std::endl;
    std::cerr << "This can be fixed in several ways, for example" << std::endl
              << "* adding " << toolname << " to the directory that contains this tool, " << *selfpath << std::endl
              << "* adding the directory that contains " << toolname << " to the PATH environment variable" << std::endl
              << "* adding " << toolname << " to the current directory" << std::endl
            ;
    exit(EX_CONFIG);
}

} // namespace sratools
