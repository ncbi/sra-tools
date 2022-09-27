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
#if 0
#include "file-path.posix.hpp"

namespace POSIX {

FilePath::FilePath(char const *const path, ssize_t length) noexcept
: path(const_cast<char *>(path))
, length(length)
, owns(false)
{
    if (path) {
        if (length < 0) {
            while (this->path[0] == '/' && this->path[1] == '/')
                ++this->path;
        }
        else {
            auto const e = this->path + this->length;
            while (this->path + 1 < e && this->path[0] == '/' && this->path[1] == '/') {
                ++this->path;
                --this->length;
            }
        }
    }
}

bool FilePath::access(int flags) const {
    assert(length < 0 || path[length] == '\0');
    return ::access(path, flags) == 0;
}

bool FilePath::exists(FilePath const &name)
{
    return ::access(name.path, F_OK) == 0;
}

static void changeDirectory(char const *dir) {
    if (chdir(dir) == 0)
        return;

    throw std::system_error(errno, std::system_category(), std::string("chdir: ") + dir);
}

FilePath FilePath::cwd()
{
    auto const path = getcwd(NULL, 0); // this is documented to work on macOS, Linux, and BSD
    auto result = FilePath(path);

    result.owns = true;
    result.isCwd = true;

    return result;
}

void FilePath::makeCurrentDirectory(FilePath const &haveCwd) const
{
    assert(haveCwd.isCwd);
    assert(length < 0 || path[length] == '\0');
    changeDirectory(path);
    haveCwd.isCwd = !(isCwd = true);
}

FilePath FilePath::makeCurrentDirectory() const
{
    FilePath const &cur = cwd();

    assert(length < 0 || path[length] == '\0');
    changeDirectory(path);

    return cur;
}

static size_t countTrailingSep(size_t length, char const *str) {
    size_t n = 0;
    while (n < length && str[length - (n + 1)] == '/')
        ++n;
    return n;
}

static size_t countLeadingSep(size_t length, char const *str) {
    size_t i;
    for (i = 0; i < length && str[i] == '/'; ++i)
        ;
    return i;
}

static size_t withoutTrailingSep(size_t length, char const *str) {
    auto const n = countTrailingSep(length, str);
    return n < length ? (length - n) : 1;
}

static size_t countLeadingSep(size_t length, char const *str, bool protectRoot) {
    auto const n = countLeadingSep(length, str);
    return (protectRoot || n == length) ? (n - 1) : n;
}

FilePath FilePath::append(FilePath const &leaf) const {
    FilePath result;

    auto path = this->path;
    auto length = this->length < 0 ? strlen(path) : (size_t)this->length;
    {
        auto const leading = countLeadingSep(length, path, true);
        length = withoutTrailingSep(length - leading, path += leading);
    }
    auto str = leaf.path;
    auto len = leaf.length < 0 ? strlen(str) : (size_t)leaf.length;
    {
        auto const leading = countLeadingSep(len, str);
        len = withoutTrailingSep(len - leading, str += leading);
    }
    if (len == 0)
        return *this;

    result.path = (char *)malloc(length + 1 + len + 1);
    if (result.path != nullptr) {
        result.owns = true;
        memcpy(result.path, path, length);
        result.path[length++] = '/';
        memcpy(result.path + length, str, len); length += len;
        result.path[length] = '\0';
        result.length = length;

        return result;
    }
    throw std::bad_alloc();
}

FilePath::FilePath(size_t length, char const *path)
{
    auto const leading = countLeadingSep(length, path, true);
    length = withoutTrailingSep(length - leading, path += leading);

    this->path = (char *)malloc(length + 1);
    assert(this->path != nullptr);
    if (this->path) {
        memcpy(this->path, path, length);
        this->path[length] = '\0';
        owns = true;
        this->length = length;
    }
    else
        throw std::bad_alloc();
}

FilePath FilePath::canonical() const
{
    FilePath result;

    assert(length < 0 || path[length] == '\0');
    if ((result.path = realpath(path, nullptr)) != nullptr) {
        result.owns = true;
        return result;
    }
    throw std::system_error(std::error_code(errno, std::system_category()), std::string("realpath: ") + path);
}

static char const *findThisExecutable(char const *argv0, char const *const *const extra)
{
#if LINUX
    // Linux provides procfs
    return "/proc/self/exe";
#elif MAC
    auto result = extra[0];

    for (auto cur = extra; *cur; ++cur) {
        if (strncmp(*cur, "executable_path=", 16) == 0) {
            result = (*cur) + 16;
            break;
        }
    }
    return result;
#else
    return argv0;
#endif
}

FilePath FilePath::fullPathToExecutable(char const *const *const argv, char const *const *const envp, char const *const *const extra)
{
    auto result = FilePath(argv[0]);
    try {
        result = FilePath(findThisExecutable(argv[0], extra)).canonical();
    }
    catch (std::system_error const &e) {
        LOG(2) << "unable to get full path to executable " << argv[0] << std::endl;
    }
    return result;
}

bool FilePath::removeSuffix(size_t const count) {
    if (owns && (length < 0 || path[length] == '\0')) {
        auto endp = length < 0 ? nullptr : (path + length);
        if (endp == nullptr) {
            endp = path;
            while (*endp != '\0')
                ++endp;
            length = endp - path;
        }
        auto const trim = endp - count;
        while (path < endp) {
            if (*--endp == '/')
                break;
            if (endp == trim) {
                *endp = '\0';
                length = endp - path;
                return true;
            }
        }
    }
    return false;
}

bool FilePath::removeSuffix(char const *const suffix, size_t const count) {
    if (owns && (length < 0 || path[length] == '\0') && memchr(suffix, '/', count) == nullptr) {
        size_t const len = length < 0 ? strlen(path) : length;
        if (count < len && memcmp(path + (len - count), suffix, count) == 0) {
            length = len - count;
            path[length] = '\0';
            return true;
        }
    }
    return false;
}

} // namespace POSIX

#else
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

unsigned FilePath::size() const {
    return (unsigned)trimPath(path).size();
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
#endif
