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

#include <vector>
#include <string>
#include <system_error>
#include "util.hpp"

#if USE_WIDE_API
using NativeString = std::wstring;
#else
using NativeString = std::string;
#endif

struct FilePath {
private:
    NativeString path;

    void changeDirectory() const;
public:
#if MS_Visual_C
    FilePath() {};
    FilePath(FilePath const &other) : path(other.path) {}
    FilePath &operator =(FilePath const &other) { path = other.path; return *this; }
#else
    FilePath() = default;
    FilePath(FilePath const &) = default;
    FilePath(FilePath &&) = default;
    FilePath &operator =(FilePath const &) = default;
    FilePath &operator =(FilePath &&) = default;
#endif

    explicit FilePath(NativeString const &in);
#if USE_WIDE_API
    explicit FilePath(std::string const &in);
#endif

    FilePath copy() const { return *this; }

    operator std::string() const;
#if USE_WIDE_API
    operator std::wstring() const;
#endif
    NativeString const &rawValue() const { return path; }

    size_t size() const;
    bool empty() const { return path.empty(); }
    operator bool() const { return !empty(); }

    bool isSimpleName() const;
    bool isSameFileSystemObject(FilePath const &other) const;

    bool executable() const;

    bool readable() const;

    bool exists() const;
    static bool exists(std::string const &);

    /// split a path into dirname and basename
    std::pair< FilePath, FilePath > split() const;

    FilePath baseName() const { return split().second; };
    static char const *baseName(char const *path) {
        auto last = path;
        for ( ; ; ) {
            auto const ch = *path++;
            if (ch == '\0')
                break;
            if (ch == '/')
                last = path;
#if WINDOWS
            if (ch == '\\')
                last = path;
#endif
        }
        return last;
    }

    /// A new `FilePath` with path element appended
    FilePath append(FilePath const &element) const;
    FilePath append(std::string const &element) const { return this->append(FilePath(element)); }

    /// Remove `count` code units from end of last path element
    bool removeSuffix(size_t count);

    /// Remove `suffix` if last path element ends with `suffix`
    bool removeSuffix(std::string const &suffix);

    static FilePath cwd();

    /// Try to make this the current working directory.
    /// Returns: true if successful.
    bool makeCurrentDirectory() const {
        try {
            changeDirectory();
            return true;
        }
        catch (std::system_error const &e) {
            return false;
        }
    }

    static FilePath fullPathToExecutable(char const *const *argv, char const *const *envp, char const *const *extra = nullptr);
#if USE_WIDE_API
    static FilePath fullPathToExecutable(wchar_t const *const *argv, wchar_t const *const *envp, char const *const *extra = nullptr);
#endif
};
