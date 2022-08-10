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

#include "util.hpp"
#include "util.win32.hpp"
#include "file-path.win32.hpp"
#include <ios>
#include <new>

#define NUL_W (L'\0')
#define DOT_W (L'.')
#define SEP_W (L'\\')

#define CONST_CAST_W(X) (reinterpret_cast<wchar_t const *>(X))
#define CAST_W(X) (reinterpret_cast<wchar_t *>(X))

#define NUL_N ('\0')
#define DOT_N ('.')
#define SEP_N ('\\')
#define SEP_POSIX ('/')

#define CONST_CAST_N(X) (reinterpret_cast<char const *>(X))
#define CAST_N(X) (reinterpret_cast<char *>(X))

#if USE_WIDE_API
using API_CHAR = wchar_t;
#define API_IS_WIDE (true)
#define API_NUL NUL_W
#define API_SEP SEP_W
#define API_DOT DOT_W
#define SELF_DIR L"."
#else
using API_CHAR = char;
#define API_IS_WIDE (false)
#define API_NUL NUL_N
#define API_SEP SEP_N
#define API_DOT DOT_N
#define SELF_DIR "."
#endif
using API_CONST_STRING = API_CHAR const *;
using API_STRING = API_CHAR *;

#define API_CONST_CAST(X) (reinterpret_cast<API_CONST_STRING>(X))
#define API_CAST(X) (reinterpret_cast<API_STRING>(X))
#define API_CONST_PATH API_CONST_CAST(this->path)
#define API_PATH API_CAST(this->path)
#define WIDE_CONST_PATH CONST_CAST_W(this->path)
#define NARROW_CONST_PATH CONST_CAST_N(this->path)
#define WIDE_PATH CAST_W(this->path)
#define NARROW_PATH CAST_N(this->path)

#define WIDE(F) wide().F
#define NARROW(F) narrow().F
#define COPY(F) copy().F

template < typename T >
static Win32Support::auto_free_ptr< T > newAutoFree(size_t count)
{
    auto const tmp = (T *)malloc(sizeof(T) * count);
    if (tmp)
        return Win32Support::auto_free_ptr< T >(tmp);
    throw std::bad_alloc();
}

static inline size_t measure(char const *const string)
{
    return strlen(string);
}

static inline size_t measure(wchar_t const *const string)
{
    return wcslen(string);
}

static inline size_t measure(void const *const string, bool isWide)
{
    return isWide ? measure(CONST_CAST_W(string)) : measure(CONST_CAST_N(string));
}

static inline bool isDirSeperator(char const ch)
{
    return ch == '\\' || ch == '/';
}

static inline bool isDirSeperator(wchar_t const ch)
{
    return ch == L'\\' || ch == L'/';
}

static inline bool isPathSeperator(API_CHAR const ch)
{
#if USE_WIDE_API
    return isDirSeperator(ch) || ch == L':';
#else
    return isDirSeperator(ch) || ch == ':';
#endif
}

static inline size_t getFullPathName(void const *path, size_t length = 0, void *result = nullptr) {
    API_CHAR dummy;
#if USE_WIDE_API
    auto const len = GetFullPathNameW
#else
    auto const len = GetFullPathNameA
#endif
                                      (reinterpret_cast<API_CONST_STRING>(path)
                                      , length
                                      , length == 0 ? &dummy : reinterpret_cast<API_STRING>(result)
                                      , nullptr);
    if (len == 0)
        throw_system_error("GetFullPathName");
    return len;
}

static inline bool pathFileExists(void const *path)
{
    auto const y =
#if USE_WIDE_API
        PathFileExistsW
#else
        PathFileExistsA
#endif
                        (reinterpret_cast<API_CONST_STRING>(path));
    return y == TRUE;
}

static inline HANDLE createFile(void const *path, int mode, int share, int createMode, int createFlags)
{
    return
#if USE_WIDE_API
        CreateFileW
#else
        CreateFileA
#endif
                    (reinterpret_cast<API_CONST_STRING>(path)
                     , mode
                     , share
                     , NULL
                     , createMode
                     , createFlags
                     , NULL);
}

static inline bool isExecutable(void const *path)
{
    DWORD dummy = 0;
    return
#if USE_WIDE_API
        GetBinaryTypeW
#else
        GetBinaryTypeA
#endif
                        (reinterpret_cast<API_CONST_STRING>(path), &dummy) == 0 ? false : true;

}

static inline void setCurrentDirectory(API_CONST_STRING path)
{
    if (
#if USE_WIDE_API
        SetCurrentDirectoryW
#else
        SetCurrentDirectoryA
#endif
                            (path) != TRUE)
        throw_system_error("chdir");
}

static inline size_t getModuleFileName(API_STRING buffer, size_t size)
{
    return
#if USE_WIDE_API
        GetModuleFileNameW
#else
        GetModuleFileNameA
#endif
                            (NULL, buffer, size);
}

static inline API_CONST_STRING pathFindNextComponent(API_CONST_STRING path)
{
#if USE_WIDE_API
    return PathFindNextComponentW(path);
#else
    return PathFindNextComponentA(path);
#endif
}

namespace Win32 {

// in code units
ssize_t FilePath::measure() const {
    if (path == nullptr)
        return -1;
    
    if (length >= 0)
        return length;
    
    return ::measure(path, isWide);
}

FilePath FilePath::narrow() const
{
    FilePath result = *this;
    
    if (isWide && path != nullptr && length != 0) {
        auto const ptr = CONST_CAST_W(path);

        if (length < 0) {
            result.length = Win32Support::unwideSize(ptr, length) - 1; // returned length is +1 for NUL
            assert(result.length > 0);
        }
        else {
            result.length = length;
        }
        result.path = malloc(sizeof(char) * (result.length + 1));
        if (result.path == nullptr)
            throw std::bad_alloc();
        
        result.owns = true;
        auto const actlen = Win32Support::unwiden(CAST_N(result.path), result.length + 1, ptr, length);
        assert(actlen > 0);
        result.length = actlen;
        result.isWide = false;
    }
    return result;
}

FilePath FilePath::wide() const
{
    FilePath result = *this;
    
    if (!isWide && path != nullptr && length != 0) {
        auto const ptr = CONST_CAST_N(path);

        if (length < 0) {
            result.length = Win32Support::wideSize(ptr, length) - (length < 0 ? 1 : 0);
            assert(result.length > 0);
        }
        else {
            result.length = length;
        }
        result.path = malloc(sizeof(wchar_t) * (result.length + 1));
        if (result.path == nullptr)
            throw std::bad_alloc();
        
        result.owns = true;
        auto const actlen = Win32Support::widen(CAST_W(result.path), result.length + 1, ptr, length);
        assert(actlen > 0);
        result.length = actlen;
        result.isWide = true;
    }
    return result;
}

FilePath FilePath::copy() const
{
    return FilePath(measure(), isWide, isFromOS, path);
}

FilePath::FilePath(size_t length, bool isWide, bool isFromOS, void const *path)
: isWide(isWide)
{
    size_t const elem_bytes = isWide ? sizeof(wchar_t) : sizeof(char);
    this->path = malloc((length + 1) * elem_bytes);
    if (this->path == nullptr)
        throw std::bad_alloc();
    this->owns = true;
    this->isFromOS = isFromOS;
    memcpy(this->path, path, length * elem_bytes);
    owns = true;
    if (isWide)
        WIDE_PATH[length] = NUL_W;
    else
        NARROW_PATH[length] = NUL_N;
}

/// A new `FilePath` with a canonical copy of the path.
FilePath FilePath::canonical() const
{
    if (path == nullptr || length == 0)
        throw std::logic_error(std::string("FilePath::canonical() called with an empty path"));

#if USE_WIDE_API
    if (!isWide)
        return WIDE(canonical)();
#else
    if (isWide)
        return NARROW(canonical)();
#endif
    if (!isTerminated())
        return COPY(canonical)();

    FilePath result;
    auto const need = getFullPathName(path);
    result.path = malloc(need * sizeof(API_CHAR));
    if (result.path == nullptr)
        throw std::bad_alloc();

    result.owns = true;
    result.length = getFullPathName(path, need, result.path);
    result.isWide = API_IS_WIDE;
    result.isFromOS = true;

    return result;
}

FilePath::operator std::string() const
{
    auto result = std::string();
    
    if (path == nullptr || length == 0)
        ;
    else if (isWide)
        result = std::string(narrow());
    else {
        auto const ptr = NARROW_CONST_PATH;
        auto const len = measure();

        result.reserve(len);
        for (auto const &ch : make_sequence(ptr, len)) {
            result.append(1, ch == SEP_N ? SEP_POSIX : ch);
        }
    }
    return result;
}

#if USE_WIDE_API
FilePath::operator std::wstring() const
{
    if (path == nullptr || length == 0)
        return std::wstring();
    
    if (isWide) {
        auto const ptr = WIDE_CONST_PATH;
        auto const len = measure();

        return std::wstring(ptr, ptr + len);
    }
    else {
        return std::wstring(wide());
    }
}
#endif

bool FilePath::exists() const {
    if (path == nullptr || length == 0)
         return false;
#if USE_WIDE_API
    if (!isWide)
        return WIDE(exists)();
#else
    if (isWide)
        return NARROW(exists)();
#endif
    if (!isTerminated())
        return COPY(exists)();

    return pathFileExists(path);
}

bool FilePath::readable() const {
    if (path == nullptr || length == 0)
        return false;
#if USE_WIDE_API
    if (!isWide)
        return WIDE(readable)();
#else
    if (isWide)
        return NARROW(readable)();
#endif
    if (!isTerminated())
        return COPY(readable)();
    if (!this->exists())
        return false;

    auto ff = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS;
    do {
        auto const fh = createFile(path
                                   , GENERIC_READ
                                   , FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE
                                   , OPEN_EXISTING
                                   , ff);
        if (fh != INVALID_HANDLE_VALUE) {
            CloseHandle(fh);
            return true;
        }
        auto const err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || ff == FILE_ATTRIBUTE_NORMAL)
            return false;
        ff = FILE_ATTRIBUTE_NORMAL;
    } while (1);
}

bool FilePath::executable() const {
    if (path == nullptr || length == 0)
        return false;
#if USE_WIDE_API
    if (!isWide)
        return WIDE(executable)();
#else
    if (isWide)
        return NARROW(executable)();
#endif
    if (!isTerminated())
        return COPY(executable)();
    if (!this->exists())
        return false;

    return isExecutable(path);
}

FilePath FilePath::cwd() {
    auto result = FilePath(SELF_DIR).canonical();
    result.isCwd = true;
    result.isFromOS = true;
    return result;
}

void FilePath::changeDirectory() const {
    assert(path != nullptr && length != 0);
    if (path == nullptr || length == 0)
        throw std::logic_error("can't set current directory to an empty path");
#if USE_WIDE_API
    if (!isWide)
        WIDE(changeDirectory)();
#else
    if (isWide)
        NARROW(changeDirectory)();
#endif
    
    // WTH!!! SetCurrentDirectory demands the path ends with backslash.
    // WTH!!! And it can modify its input! So we have to make a modifiable copy.
    auto size = measure();
    auto const wth = newAutoFree<API_CHAR>(size + 2);
    auto const ptr = wth.get();
    memcpy(ptr, path, size * sizeof(API_CHAR));
    if (ptr[size - 1] != API_SEP)
        ptr[size++] = API_SEP;
    ptr[size] = API_NUL;
    
    setCurrentDirectory(ptr);
    isCwd = true;
}

void FilePath::makeCurrentDirectory(FilePath const &haveCwd) const {
    assert(haveCwd.isCwd);
    if (!haveCwd.isCwd)
        throw std::logic_error("that is not the current working directory");
    
    changeDirectory();
    haveCwd.isCwd = !isCwd;
}

FilePath FilePath::makeCurrentDirectory() const {
    auto result = FilePath::cwd();
    makeCurrentDirectory(result);
    return result;
}


static inline API_STRING fullPathToExe()
{
    DWORD size = 4096;
    API_CHAR *buffer = (API_CHAR *)malloc(size * sizeof(*buffer));
    
    while (buffer != nullptr) {
        auto const sz = getModuleFileName(buffer, size);
        if (sz < size)
            break;
        free(buffer);
        buffer = (API_CHAR *)malloc((size *= 2) * sizeof(*buffer));
    }
    return buffer;
}

FilePath FilePath::fullPathToExecutable(char const *const *const argv, char const *const *const envp, char const *const *const extra)
{
    FilePath result;

    result.path = fullPathToExe();
    assert(result.path != nullptr);
    if (result.path == nullptr)
        throw std::bad_alloc();
    result.owns = true;
    result.isWide = API_IS_WIDE;
    result.isFromOS = true;

    return result;

    (void)(extra);
    (void)(envp);
    (void)(argv);
}

#if USE_WIDE_API
FilePath FilePath::fullPathToExecutable(wchar_t const *const *const argv, wchar_t const *const *const envp, char const *const *const extra)
{
    FilePath result;
    
    result.path = fullPathToExe();
    assert(result.path != nullptr);
    if (result.path == nullptr)
        throw std::bad_alloc();
    result.owns = true;
    result.isWide = true;
    result.isFromOS = true;

    return result;

    (void)(extra);
    (void)(envp);
    (void)(argv);
}
#endif

template <typename T>
static T const *FilePathSplitAt(T const *const path, size_t const length)
{
    auto at = path + length;
    while (at > path) {
        if (isPathSeperator(at[-1]))
            return at;
        --at;
    }
    return path;
}

std::pair<FilePath, FilePath> FilePath::split() const {
#if USE_WIDE_API
    if (isFromOS && !isWide)
        return WIDE(split)();
#else
    if (isFromOS && isWide)
        return NARROW(split)();
#endif
    FilePath left, right;

    if (path == nullptr || length == 0)
        ;
    else if (isFromOS) {
#if USE_WIDE_API
        assert(isWide);
#else
        assert(!isWide);
#endif
        auto last = API_CONST_PATH;
        auto first = last;
        auto endp = first + measure();
        while (first + 1 < endp && isDirSeperator(endp[-1]))
            --endp;
        auto next = pathFindNextComponent(last);
        
        if (next == nullptr) // there is only one path component
            right = *this;
        else {
            left.isFromOS = true;
            right.isFromOS = true;
            while (next < endp && *next != API_NUL) {
                last = next;
                next = pathFindNextComponent(last);
            }
            left.path = (void *)first;
            left.length = (last == first + 1) ? 1 : ((last - first) - 1);
            right.path = (void *)last;
            right.length = endp - last;
        }
    }
    else if (isWide) {
        auto const start = CONST_CAST_W(this->path);
        auto endp = start + measure();
        while (start + 1 < endp && isDirSeperator(endp[-1]))
            --endp;
        auto const at = FilePathSplitAt(start, endp - start);
        
        if (at == start)
            right = *this;
        else {
            left.path = (void *)start;
            left.length = at - start;
            while (left.length > 1 && isDirSeperator(start[left.length]))
                --left.length;
            right.path = (void *)at;
            right.length = endp - at;
        }
    }
    else {
        auto const start = CONST_CAST_N(this->path);
        auto endp = start + measure();
        while (start + 1 < endp && isDirSeperator(endp[-1]))
            --endp;
        auto const at = FilePathSplitAt(start, endp - start);
        
        if (at == start)
            right = *this;
        else {
            left.path = (void *)start;
            left.length = at - start;
            while (left.length > 1 && isDirSeperator(start[left.length]))
                --left.length;
            right.path = (void *)at;
            right.length = endp - at;
        }
    }
    return {left, right};
}

template <typename T>
static void rtrimPath(T const *&str, size_t &len)
{
    while (len > 1 && isDirSeperator(str[len - 1]))
        --len;
}

template <typename T>
static void trimPath(T const *&str, size_t &len)
{
    while (len > 0 && isDirSeperator(str[len - 1]))
        --len;
    while (len > 0 && isDirSeperator(str[0])) {
        --len;
        ++str;
    }
}

template <typename T>
static std::pair<void *, size_t> FilePathAppend(T const *left, size_t llen, T const *right, size_t rlen, T const sep, T const nul)
{
    rtrimPath<T>(left, llen); // remove any trailing path seperators
    trimPath<T>(right, rlen); // remove any leading or trailing path seperators
    
    if (rlen == 0)
        return {nullptr, 0};
    
    auto const need = llen + 1 + rlen + 1;
    auto const rslt = malloc(need * sizeof(T));
    if (rslt == nullptr)
        throw std::bad_alloc();
    
    auto dst = reinterpret_cast<T *>(rslt);
    memcpy(dst, left, llen); dst += llen;
    *dst++ = sep;
    memcpy(dst, right, rlen); dst += rlen;
    *dst++ = nul;
    
    return {rslt, need - 1};
}

FilePath FilePath::append(FilePath const &leaf) const {
    if (leaf.empty())
        return *this;
    if (empty())
        return leaf;
    if (isWide && !leaf.isWide)
        return append(leaf.wide());
    if (!isWide && leaf.isWide)
        return append(leaf.narrow());

    if (isFromOS) {
        if (!isWide)
            return WIDE(append)(leaf);
        if (!leaf.isWide)
            return append(leaf.wide());
        if (!isTerminated())
            return COPY(append)(leaf);
        if (!leaf.isTerminated())
            return append(leaf.copy());
        
        FilePath result;
        auto const lpath = CONST_CAST_W(leaf.path);
        auto const path = CONST_CAST_W(this->path);
        wchar_t *rpath = nullptr;
        auto const rc = PathAllocCombine(path, lpath, PATHCCH_FORCE_ENABLE_LONG_NAME_PROCESS|PATHCCH_ALLOW_LONG_PATHS, &rpath);
        if (rc != S_OK)
            throw std::system_error(std::error_code(int(rc & 0xFFFF), std::system_category()), "PathAllocCombine");

        result.length = ::measure(rpath);
        result.path = malloc((result.length + 1) * sizeof(wchar_t));
        if (result.path != nullptr)
            memcpy(result.path, rpath, (result.length + 1) * sizeof(wchar_t));
        LocalFree(rpath);
        if (result.path == nullptr)
            throw std::bad_alloc();
        
        result.isWide = true;
        result.owns = true;
        result.isFromOS = true;
        return result;
    }
    else if (isWide) {
        auto const pl = FilePathAppend(CONST_CAST_W(this->path), measure(), CONST_CAST_W(leaf.path), leaf.measure(), SEP_W, NUL_W);
        if (pl.second == 0)
            return *this;

        FilePath result;
        result.path = pl.first;
        result.length = pl.second;
        result.isWide = true;
        result.owns = true;
        return result;
    }
    else {
        auto const pl = FilePathAppend(CONST_CAST_N(this->path), measure(), CONST_CAST_N(leaf.path), leaf.measure(), SEP_N, NUL_N);
        if (pl.second == 0)
            return *this;

        FilePath result;
        result.path = pl.first;
        result.length = pl.second;
        result.isWide = false;
        result.owns = true;
        return result;
    }
}

bool FilePath::removeSuffix(size_t count) {
    if (!owns || count == 0) return false;

    auto len = measure();
    if (len < (ssize_t)count) return false;

    if (isWide) {
        auto const path = WIDE_CONST_PATH;
        while (count > 0 && len > 0) {
            if (path[len - 1] == SEP_W)
                return false;
            --len;
            --count;
        }
        while (len > 1 && path[len - 1] == SEP_W)
            --len;
        length = len;
    }
    else {
        auto const path = NARROW_CONST_PATH;
        while (count > 0 && len > 0) {
            if (path[len - 1] == SEP_N)
                return false;
            --len;
            --count;
        }
        if (count != 0)
            return false;
        while (len > 1 && path[len - 1] == SEP_N)
            --len;
        length = len;
    }
    return true;
}

template <typename T>
static ssize_t remove_suffix(T const *const suffix, size_t length, T const *const str, size_t len, T const sep)
{
    if (len < length)
        return -1;
    while (len > 0 && length > 0) {
        if (suffix[length - 1] == sep || suffix[length - 1] != str[len - 1])
            return -1;
        --len;
        --length;
    }
    assert(length == 0);
    while (len > 1 && str[len - 1] == sep)
        --len;
    return (ssize_t)len;
}

bool FilePath::removeSuffix(char const *const suffix, size_t length) {
    if (!owns) return false;

    if (isWide) {
        // we are wide, change the suffix to match us
        auto const wlen = Win32Support::wideSize(suffix, length);
        auto const wstr = newAutoFree<wchar_t>(wlen + 1);
        Win32Support::widen(wstr.get(), wlen + 1, suffix, length);
        return removeSuffix(wstr.get(), wlen);
    }
    auto const newlen = remove_suffix(suffix, length, NARROW_CONST_PATH, measure(), SEP_N);
    if (newlen < 0)
        return false;
    this->length = newlen;
    return true;
}

bool FilePath::removeSuffix(wchar_t const *const suffix, size_t length) {
    if (!owns) return false;

    if (isWide) {
        auto const newlen = remove_suffix(suffix, length, WIDE_CONST_PATH, measure(), SEP_W);
        if (newlen < 0)
            return false;
        this->length = newlen;
        return true;
    }
    // we are not wide, change the suffix to match us
    auto const len = Win32Support::unwideSize(suffix, length);
    auto const str = newAutoFree<char>(len + 1);
    Win32Support::unwiden(str.get(), len + 1, suffix, length);
    return removeSuffix(str.get(), len);
}

} // namespace Win32
#else
#endif
