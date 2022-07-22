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

#pragma once

#if WINDOWS
#else

#include <system_error>
#include <cstring>
#include <cassert>

namespace POSIX {

struct FilePath {

    /// creates a non-owning object
    FilePath(FilePath const &other)
    : path(other.path)
    , length(other.length)
    , owns(false)
    , isCwd(other.isCwd)
    {}

    FilePath &operator= (FilePath const &other) {
        assert(other.owns == false); // this needs to be a move assignment

        auto const oldpath = owns ? path : nullptr;
        owns = false;
        path = other.path;
        length = other.length;
        isCwd = other.isCwd;

        if (oldpath) {
            assert(oldpath != path);
            free(oldpath);
        }

        return *this;
    }

    /// other loses any ownership it may have had
    FilePath(FilePath &&other)
    : path(other.path)
    , length(other.length)
    , owns(other.owns)
    , isCwd(other.isCwd)
    {
        other.owns = false;
    }
    FilePath &operator= (FilePath &&other) {
        if (!other.owns)
            return *this = (FilePath const &)other;

        auto const oldpath = owns ? path : nullptr;
        path = other.path;
        length = other.length;
        owns = true;
        isCwd = other.isCwd;
        other.owns = false;

        if (oldpath && oldpath != path)
            free(oldpath);

        return *this;
    }

    ~FilePath() {
        if (owns)
            free(path);
    }
    char const *get() const { return path; }

    FilePath() = default;
    /// creates a non-owning object
    explicit FilePath(char const *const path, ssize_t length = -1) noexcept;

    /// A new `FilePath` with a canonical copy of the path.
    FilePath canonical() const;

    /// A new `FilePath` that owns its path.
    FilePath copy() const {
        return FilePath(length < 0 ? strlen(path) : length, path);
    }

    bool removeSuffix(size_t const count);
    bool removeSuffix(char const *const suffix, size_t length);
    bool removeSuffix(FilePath const &baseName) {
        return removeSuffix(baseName.path, baseName.length < 0 ? strlen(baseName.path) : baseName.length);
    }

    size_t size() const { return length < 0 ? strlen(path) : (size_t)length; }

    operator std::string() const { return std::string(path, path + size()); }

    bool empty() const { return path == nullptr || length == 0; }

    bool readable() const {
        return (path != nullptr && length != 0) ? access(R_OK) : false;
    }

    bool exists() const {
        return (path != nullptr && length != 0) ? access(F_OK) : false;
    }

    bool executable() const {
        return (path != nullptr && length != 0) ? access(X_OK) : false;
    }

    static bool exists(FilePath const &name);

    static FilePath cwd();

    void makeCurrentDirectory(FilePath const &haveCwd) const;
    FilePath makeCurrentDirectory() const;

    std::pair<FilePath, FilePath> split() const
    {
        auto const sep = lastSep();
        if (sep < 0)
            return std::make_pair(FilePath(nullptr, 0), *this);
        if (sep == 0)
            return {FilePath(path, 1), FilePath(path + sep + 1)};
        else
            return {FilePath(path, sep), FilePath(path + sep + 1)};
    }

    FilePath append(FilePath const &leaf) const;

    static FilePath fullPathToExecutable(char const *const *const argv, char const *const *const envp, char const *const *const extra = nullptr);

private:
    char *path = nullptr;
    ssize_t length = -1;
    bool owns = false;
    mutable bool isCwd = false;

    bool access(int flags) const;

    /// \returns index in path of the next separator at or after pos
    size_t nextSep(size_t pos) const
    {
        for (ssize_t i = pos; i < length || length < 0; ++i) {
            auto const ch = path[i];
            switch (ch) {
            case '/':
            case '\0':
                return i;
            }
        }
        return length;
    }

    ssize_t lastSep() const {
        ssize_t result = -1;
        for (ssize_t i = nextSep(0); (i < length || length < 0) && path[i]; i = nextSep(i + 1))
            result = i;
        return result;
    }

    FilePath(size_t length, char const *path);
};

} // namespace POSIX

using PlatformFilePath = POSIX::FilePath;

#endif
