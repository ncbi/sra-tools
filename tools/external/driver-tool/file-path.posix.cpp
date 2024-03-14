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
#include "util.hpp"
#include "file-path.hpp"
#include <algorithm>
#include <vector>
#include <string>

static NativeString::size_type schemeLength(NativeString const &path)
{
    for (auto const &ch : path) {
        if (ch == ':')
            return &ch - &path[0];
        
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
            continue;

        if (&ch == &path[0]) // first character of scheme must be alpha
            break;

        if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '.')
            continue;

        break;
    }
    return 0;
}

FilePath::FilePath(NativeString const &in) : path(in) {}

bool FilePath::isSimpleName() const {
    if (schemeLength(path) != 0)
        return false;

    if (path.find_first_of('/') != NativeString::npos)
        return false;

    return true;
}

static std::string trimPath(std::string const &path, bool const issecond = false)
{
    auto end = path.size();
    auto start = decltype(end)(0);

    while (start + 1 < end && path[start] == '/' && path[start + 1] == '/')
        ++start;

    if (issecond && path[start] == '/')
        ++start;

    while (end > 0 && end - 1 > start && path[end - 1] == '/')
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
        auto const newlen = path.size() - count;
        auto const no_sep = path.find('/', newlen) == std::string::npos;
        if (no_sep) {
            path.resize(newlen);
            return true;
        }
    }
    return count == 0;
}

bool FilePath::removeSuffix(std::string const &suffix)
{
    if (suffix.empty() || ends_with(suffix, path))
        return removeSuffix(suffix.size());
    return false;
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

// In Darwin, `main` takes 4 parameters, the last one being this `apple` array.
static char const *searchExtra(char const *const *const apple)
{
    // In modern Darwin, some element of the `apple` array
    // (usually the first) will be
    // `executable_path=<path/to/exe>`
    for (auto cur = apple; *cur; ++cur) {
        if (strncmp(*cur, "executable_path=", 16) == 0)
            return (*cur) + 16;
    }
    // In older Darwin, the first element of the `apple` array
    // will be `<path/to/exe>`
    return *apple;
}

static FilePath realPathToExecutable(char const *path)
{
    auto rp = realpath(path, nullptr);
    if (rp) {
        auto && result = FilePath(rp);
        free(rp);
        return result;
    }
    LOG(3) << "unable to get real path to executable " << path << std::endl;
    throw std::runtime_error("not found");
}

static FilePath realPathToExecutable(std::string const &path)
{
    return realPathToExecutable(path.c_str());
}

static FilePath realPathToExecutable(FilePath const &path)
{
    return realPathToExecutable(std::string(path));
}

/// Find full real path to the current executable.
///
/// First tries any platform specific means,
/// then tries `argv[0]` if it is not a bare name,
/// else searches `PATH` environment variable.
/// Finallly, it returns `argv[0]`, which will probably not be useful.
///
/// Platform specific means:
/// Linux has `/proc/self/exe`.
/// Darwin has an extra parameter to `main`.
FilePath FilePath::fullPathToExecutable(char const *const *const argv, char const *const *const envp, char const *const *const extra)
{
    try {
#if LINUX
        return realPathToExecutable("/proc/self/exe");
#elif MAC
        if (extra) {
            auto const found = searchExtra(extra);
            if (found)
                return realPathToExecutable(found);
        }
#endif
        char const *last = nullptr;
        for (auto cur = argv[0]; *cur; ++cur) {
            if (*cur == '/')
                last = cur;
        }
        if (last && *last)
            return realPathToExecutable(argv[0]);
    }
    catch (std::runtime_error) {}

    // search PATH
    FilePath const baseName(argv[0]);

    auto const PATH = getenv("PATH");
    for (auto path = PATH; PATH && *path; ++path) {
        auto cur = path;
        while (*path && *path != ':')
            ++path;
        assert(*path == '\0' || *path == ':');
        
        try {
            return realPathToExecutable(FilePath(std::string{cur, path}).append(baseName));
        }
        catch (std::runtime_error) {}
    }
    
    LOG(2) << "unable to get real path to executable " << argv[0] << std::endl;
    return baseName;
}

bool FilePath::isSameFileSystemObject(FilePath const &other) const
{
    struct stat st1, st2;

    if (stat(path.c_str(), &st1) == 0 && stat(other.path.c_str(), &st2) == 0)
        return st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;

    return false;
}

#endif
