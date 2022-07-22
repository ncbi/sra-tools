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
#include <utility>

#include "file-path.posix.hpp"
#include "file-path.win32.hpp"

template <typename FilePathImpl>
struct FilePath_ : protected FilePathImpl {
    FilePath_() = default;

    /// @brief creates a non-owning object
    /// @param length is the number of code units in path, not bytes or characters (since a character may be encoded in 4 code units in utf-8)
    explicit FilePath_(char const *const path, ssize_t length = -1) noexcept
    : FilePathImpl(path, length)
    {}

    FilePath_(std::string const &path) noexcept
    : FilePathImpl(path.data(), path.size())
    {}

#if USE_WIDE_API
    /// @brief creates a non-owning object
    /// @param length is the number of code units in path, not bytes or characters (since a character may be encoded in 2 code units in utf-16)
    explicit FilePath_(wchar_t const *const wpath, ssize_t length = -1) noexcept
    : FilePathImpl(wpath, length)
    {}

    explicit FilePath_(std::wstring const &path) noexcept
    : FilePathImpl(path.data(), path.size())
    {}
#endif

#if MS_Visual_C
    /// creates a non-owning object
    FilePath_(FilePath_ const& other) noexcept : FilePathImpl(other) {}
    FilePath_& operator= (FilePath_ const& other) noexcept {
        static_cast<FilePathImpl &>(*this) = static_cast<FilePathImpl const &>(other);
        return *this;
    }

    /// other loses any ownership it may have had
    FilePath_(FilePath_&& other) noexcept : FilePathImpl(other) {}
    FilePath_& operator= (FilePath_&& other) noexcept {
        static_cast<FilePathImpl&>(*this) = std::move(static_cast<FilePathImpl &&>(other));
        return *this;
    }
#else
    /// creates a non-owning object
    FilePath_(FilePath_ const &other) = default;
    FilePath_ &operator= (FilePath_ const &other) = default;

    /// other loses any ownership it may have had
    FilePath_(FilePath_ &&other) noexcept = default;
    FilePath_ &operator= (FilePath_ &&other) noexcept = default;
#endif

    /// A new `FilePath_` that owns its path.
    FilePath_ copy() const { return FilePathImpl::copy(); }

    /// A new `FilePath_` with a canonical copy of the path.
    FilePath_ canonical() const { return FilePathImpl::canonical(); }

    /// split a path into dirname and basename
    std::pair<FilePath_, FilePath_> split() const {
        auto both = FilePathImpl::split();
        return {both.first, both.second};
    }

    FilePath_ baseName() const {
        return split().second;
    }

    /// A new `FilePath_` with path seperator and path element appended
    FilePath_ append(FilePath_ const &element) const {
        return FilePathImpl::append(element);
    }

    /// Remove `count` code units from end of last path element
    bool removeSuffix(size_t const count) { return FilePathImpl::removeSuffix(count); }

    /// Remove `suffix` from end of last path element
    bool removeSuffix(char const *const suffix, size_t length) {
        return FilePathImpl::removeSuffix(suffix, length);
    }
    /// Remove `suffix` from end of last path element
    bool removeSuffix(std::string const &suffix) {
        return removeSuffix(suffix.data(), suffix.size());
    }
    /// Remove `suffix` from end of last path element
    bool removeSuffix(FilePath_ const &suffix) {
        return FilePathImpl::removeSuffix(suffix.impl);
    }
#if USE_WIDE_API
    /// Remove `suffix` from end of last path element
    bool removeSuffix(wchar_t const *const suffix, size_t length) {
        return FilePathImpl::removeSuffix(suffix, length);
    }
    /// Remove `suffix` from end of last path element
    bool removeSuffix(std::wstring const &suffix) {
        return removeSuffix(suffix.data(), suffix.size());
    }
#endif

    /// @NOTE The result has POSIX path seperators even on Windows
    operator std::string() const {
        auto result = FilePathImpl::operator std::string();

        while (result.size() > 1 && result.back() == '/')
            result.pop_back();

        return result;
    }
#if USE_WIDE_API
    operator std::wstring() const { return std::wstring(implementation()); }
#endif

    /// @returns the number of code units in the path
    size_t size() const { return FilePathImpl::size(); }

    /// @returns true if the path is an empty string (or null)
    bool empty() const { return FilePathImpl::empty(); }

    /// @returns true if the file system object is readable
    bool readable() const { return FilePathImpl::readable(); }

    /// @returns true if the file system object exists
    bool exists() const { return FilePathImpl::exists(); }

    /// @returns true if the file system object is "executable"
    /// @Note On POSIX, this means "searchable" for a directory.
    bool executable() const { return FilePathImpl::executable(); }

    /// @returns true if given path exists in the current working directory
    static bool exists(FilePath_ const &name) { return FilePathImpl::exists(name); }

    /// @returns the current working directory
    static FilePath_ cwd() { return FilePathImpl::cwd(); }

    /// Make this the current working directory, if the current working directory is the given path.
    void makeCurrentDirectory(FilePath_ const &haveCwd) const { return FilePathImpl::makeCurrentDirectory(haveCwd); }

    /// @returns the previous current working directory
    FilePath_ makeCurrentDirectory() const { return FilePathImpl::makeCurrentDirectory(); }

    /// @returns the full absolute path to the executable that is this process. (best effort using OS supplied methods)
    static FilePath_ fullPathToExecutable(char const *const *const argv, char const *const *const envp, char const *const *const extra = nullptr)
    {
        return FilePathImpl::fullPathToExecutable(argv, envp, extra);
    }

#if USE_WIDE_API
    /// @returns the full absolute path to the executable that is this process. (best effort using OS supplied methods)
    static FilePath_ fullPathToExecutable(wchar_t const *const *const argv, wchar_t const *const *const envp, char const *const *const extra = nullptr)
    {
        return FilePathImpl::fullPathToExecutable(argv, envp, extra);
    }
#endif

    /// @returns the platform specific implementation of this object
    FilePathImpl const &implementation() const {
        return *static_cast<FilePathImpl const *>(this);
    }
private:
    FilePath_(FilePathImpl const &impl) : FilePathImpl(impl) {}
    FilePath_(FilePathImpl &&impl) : FilePathImpl(std::move(impl)) {}
};

using FilePath = FilePath_<PlatformFilePath>;
