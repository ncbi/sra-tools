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
 *  Provide file system path support.
 *
 */

#if WINDOWS
#else
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include "debug.hpp"
#include "file-path.hpp"
#include <algorithm>
#include <vector>
#include <string>

FilePath::FilePath(NativeString const &in) : path(in) {}

static std::string trimPath(std::string const &path, bool const issecond = false)
{
    auto end = path.size();
    auto start = decltype(end)(0);
    
    while (start + 1 < end && path[start] == '/' && path[start + 1] == '/')
        ++start;
    
    if (issecond && path[start] == '/')
        ++start;
    
    while (end - 1 > start && path[end - 1] == '/')
        --end;
    
    return path.substr(start, end - start);
}

FilePath::operator std::string() const {
    return path;
}

size_t FilePath::size() const {
    return trimPath(path).size();
}

std::pair< FilePath, FilePath > FilePath::split() const {
    auto const good = trimPath(path);
    auto const sep = good.find_last_of('/');
    if (sep == std::string::npos)
        return {FilePath(), FilePath(good)};
    if (sep == 0)
        return {FilePath("/"), FilePath(good.substr(1))};
    else
        return {FilePath(good.substr(0, sep)), FilePath(good.substr(sep + 1))};
}

bool FilePath::exists() const {
    return access(path.c_str(), F_OK) == 0;
}

bool FilePath::exists(std::string const &path) {
    return access(path.c_str(), F_OK)== 0;
}

bool FilePath::executable() const {
    return access(path.c_str(), X_OK) == 0;
}

bool FilePath::readable() const {
    return access(path.c_str(), R_OK) == 0;
}

FilePath FilePath::append(FilePath const &element) const {
    return FilePath(trimPath(path) + "/" + trimPath(element.path, true));
}

bool FilePath::removeSuffix(size_t const count) {
    if (count > 0 && path.size() >= count) {
        path.resize(path.size() - count);
        return true;
    }
    return count == 0;
}

FilePath FilePath::cwd()
{
    auto const path = getcwd(NULL, 0); // this is documented to work on macOS, Linux, and BSD
    auto result = FilePath(path);
    free(path);
    return result;
}

void FilePath::changeDirectory() const {
    if (chdir(path.c_str()) == 0)
        return;
    throw std::system_error(errno, std::system_category(), std::string("chdir: ") + path);
}

FilePath from_realpath(char const *path)
{
    auto const real = realpath(path, nullptr);
    if (real)
        return FilePath(real);
    throw std::system_error(std::error_code(errno, std::system_category()), std::string("realpath: ") + path);
}

#if MAC
static char const *find_executable_path(char const *const *const extra, char const *const argv0)
{
    for (auto cur = extra; extra && *cur; ++cur) {
        if (strncmp(*cur, "executable_path=", 16) == 0) { // usually
            return (*cur) + 16; // Usually, this is the value.
        }
    }
    return (extra && extra[0]) ? extra[0] : argv0;
}
#endif

FilePath FilePath::fullPathToExecutable(char const *const *const argv, char const *const *const envp, char const *const *const extra)
{
    char const *path;
#if LINUX
    path = "/proc/self/exe";
#elif MAC
    path = find_executable_path(extra, argv[0]);
#else
    path = argv[0];
#endif
    try {
        return from_realpath(path);
    }
    catch (std::system_error const &e) {
        LOG(2) << "unable to get full path to executable " << argv[0] << std::endl;
    }
    return FilePath(argv[0]);
}

#endif
