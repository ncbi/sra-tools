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

namespace Win32 {

struct FilePath {
    
    FilePath() = default;
    
    /// creates a non-owning object
    explicit FilePath(char const *path, ssize_t length = -1) noexcept
    : path(const_cast<void *>(reinterpret_cast<void const *>(path)))
    , length(length)
    {}

#if USE_WIDE_API
    /// @brief creates a non-owning object
    /// @param length is the number of code units in path, not bytes or characters (since a character may be encoded in 2 code units in utf-16)
    explicit FilePath(wchar_t const *wpath, ssize_t length = -1) noexcept
    : path(const_cast<void *>(reinterpret_cast<void const *>(wpath)))
    , length(length)
    , isWide(true)
    {}
#endif

    /// creates a non-owning object
    FilePath(FilePath const &other)
    : path(other.path)
    , length(other.length)
    , owns(false)
    , isWide(other.isWide)
    , isCwd(other.isCwd)
    {}
    
    FilePath &operator= (FilePath const &other) {
        assert(other.owns == false); // this needs to be a move assignment
        
        auto const oldpath = owns ? path : nullptr;
        owns = false;
        path = other.path;
        length = other.length;
        isWide = other.isWide;
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
    , isWide(other.isWide)
    , isCwd(other.isCwd)
    {
        other.owns = false;
    }
    FilePath &operator= (FilePath &&other) {
        if (!other.owns)
            return *this = (FilePath const &)other;
        
        auto const oldpath = path;
        path = other.path;
        other.owns = false;
        length = other.length;
        isWide = other.isWide;
        isCwd = other.isCwd;

        if (oldpath && oldpath != path)
            free(oldpath);
        
        return *this;
    }

    ~FilePath() {
        if (owns)
            free(path);
    }

    /// A new `FilePath` that owns its path.
    FilePath copy() const;
    
    /// A new `FilePath` with a canonical copy of the path.
    FilePath canonical() const;

    size_t size() const {
        auto const sz = measure();
        assert(sz >= 0);
        return (size_t)sz;
    }

    operator std::string() const;
#if USE_WIDE_API
    operator std::wstring() const;
#endif

    char const *get() const { return isWide ? nullptr : reinterpret_cast<char const *>(path); }
#if USE_WIDE_API
    wchar_t const *wget() const { return isWide ? reinterpret_cast<wchar_t const *>(path) : nullptr; }
#endif

    bool empty() const { return path == nullptr || length == 0; }
    
    bool readable() const;

    bool exists() const;

    bool executable() const;
    
    static bool exists(FilePath const &name) {
        return name.exists();
    }

    static FilePath cwd();
    
    void makeCurrentDirectory(FilePath const &haveCwd) const;
    FilePath makeCurrentDirectory() const;

    static FilePath fullPathToExecutable(char const *const *const argv, char const *const *const envp, char const *const *const extra = nullptr);

#if USE_WIDE_API
    static FilePath fullPathToExecutable(wchar_t const *const *const argv, wchar_t const *const *const envp, char const *const *const extra = nullptr);
#endif
    
    std::pair<FilePath, FilePath> split() const;

    FilePath append(FilePath const &leaf) const;

    bool removeSuffix(size_t const count);
    bool removeSuffix(char const *const suffix, size_t length);
    bool removeSuffix(wchar_t const *const suffix, size_t length);
    bool removeSuffix(std::string const &suffix) {
        return removeSuffix(suffix.data(), suffix.size());
    }
#if USE_WIDE_API
    bool removeSuffix(std::wstring const &suffix) {
        return removeSuffix(suffix.data(), suffix.size());
    }
    bool removeSuffix(FilePath const &baseName) {
        if (baseName.path == nullptr || baseName.length == 0) return true;
        if (path == nullptr || length == 0) return false;
        if (!isWide && baseName.isWide) return removeSuffix(baseName.narrow());
        if (isWide && !baseName.isWide) return removeSuffix(baseName.wide());
        if (isWide)
            return removeSuffix(reinterpret_cast<wchar_t const *>(baseName.path), (size_t)baseName.measure());
        else
            return removeSuffix(reinterpret_cast<char const *>(baseName.path), (size_t)baseName.measure());
    }
#else
    bool removeSuffix(FilePath const &baseName) {
        if (baseName.path == nullptr || baseName.length == 0) return true;
        if (path == nullptr || length == 0) return false;
        if (!isWide && baseName.isWide) return removeSuffix(baseName.narrow());
        if (isWide && !baseName.isWide) return removeSuffix(baseName.wide());
        if (isWide)
            return removeSuffix(reinterpret_cast<wchar_t const *>(baseName.path), baseName.measure());
        else
            return removeSuffix(reinterpret_cast<char const *>(baseName.path), baseName.measure());
    }
#endif
    
private:
    void *path = nullptr;
    ssize_t length = -1; // number of code units (not bytes or characters)
    bool owns = false;
    bool isFromOS = false;
    mutable bool isWide = false;
    mutable bool isCwd = false;
    
    bool isTerminated() const {
        if (path == nullptr) return false;
        if (length < 0) return true;
        if (isWide) return reinterpret_cast<wchar_t const *>(path)[length] == L'\0';
        return reinterpret_cast<char const *>(path)[length] == '\0';
    }
    
    // in code units
    ssize_t measure() const;

    FilePath(size_t length, bool isWide, bool isFromOS, void const *path);
    
    FilePath wide() const;
    FilePath narrow() const;
    
    void changeDirectory() const;
};

} // namespace Win32

using PlatformFilePath = Win32::FilePath;

#else
#endif
