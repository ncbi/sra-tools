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
 */

#pragma once

#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <string>
#include <stdlib.h>
#include <unistd.h>

template <typename T, typename ITER = typename T::const_iterator>
static inline bool hasPrefix(ITER start, ITER end, ITER in_start, ITER in_end)
{
    while (start != end && in_start != in_end) {
        if (!(*start == *in_start))
            return false;
        ++start;
        ++in_start;
    }
    return start == end;
}

#if __cpp_lib_starts_ends_with

static inline bool starts_with(std::string const &prefix, std::string const &in_string)
{
    return in_string.starts_with(prefix);
}

static inline bool ends_with(std::string const &suffix, std::string const &in_string)
{
    return in_string.ends_with(suffix);
}

#else

static inline bool starts_with(std::string const &prefix, std::string const &in_string)
{
    return (in_string.size() < prefix.size()) ? false : (in_string.compare(0, prefix.size(), prefix) == 0);
}

static inline bool ends_with(std::string const &suffix, std::string const &in_string)
{
    return (in_string.size() < suffix.size()) ? false : in_string.compare(in_string.size() - suffix.size(), suffix.size(), suffix) == 0;
}
#endif

static inline std::error_code error_code_from_errno()
{
    return std::error_code(errno, std::system_category());
}

static inline void throw_system_error [[noreturn]] (char const *const what)
{
    throw std::system_error(error_code_from_errno(), what);
}

static inline void throw_system_error [[noreturn]] (std::string const &what)
{
    throw std::system_error(error_code_from_errno(), what);
}

#if __cplusplus < 201703L
template <class InputIt, class T, class BinaryOp>
T reduce(InputIt const first, InputIt const last, T const init, BinaryOp && binaryOp)
{
    auto total = init;
    for (auto i = first; i != last; ++i) {
        total = binaryOp(total, *i);
    }
    return total;
}
#else
#include <numeric>
using std::reduce;
#endif

template <typename T, typename PREDICATE = std::less<T>>
class LimitChecker {
#if DEBUG || _DEBUGGING
    T const limit;
#endif
public:
    LimitChecker(T const &o)
#if DEBUG || _DEBUGGING
    : limit(o)
#endif
    {}
    void operator ()(T const &x) const {
#if DEBUG || _DEBUGGING
        PREDICATE const pred;
        if (!pred(x, limit))
            throw std::logic_error("assertion failed");
#endif
    }
};

template <typename PREDICATE>
class Checker {
#if DEBUG || _DEBUGGING
    PREDICATE const predicate;
#endif
    Checker(PREDICATE && f)
#if DEBUG || _DEBUGGING
    : predicate(std::forward<PREDICATE>(f))
#endif
    {}
    template<typename T> friend Checker<T> make_checker(T &&);
public:
    void operator ()() const {
#if DEBUG || _DEBUGGING
        if (!predicate())
            throw std::logic_error("assertion failure");
#endif
    }
};
template <typename PREDICATE>
Checker<PREDICATE> make_checker(PREDICATE && f) {
    return Checker<PREDICATE>(std::forward<PREDICATE>(f));
}

template <typename T>
class ClosedRangeChecker {
#if DEBUG || _DEBUGGING
    T const min;
    T const max;
#endif
public:
    ClosedRangeChecker(T const &min, T const &max)
#if DEBUG || _DEBUGGING
    : min(min), max(max)
#endif
    {}
    void operator ()(T const &x) const {
#if DEBUG || _DEBUGGING
        if (!(min <= x && x <= max))
            throw std::range_error("assertion failed");
#endif
    }
};

template <typename T>
class ScopeExit {
    T f;
    bool owner;
public:
    ScopeExit(T && f) : f(std::forward<T>(f)), owner(true) {}
    ScopeExit(ScopeExit const &) = delete;
    ScopeExit(ScopeExit && other)
    : f(std::forward<T>(other.f))
    , owner(other.owner)
    {
        other.owner = false;
    }
    ~ScopeExit() {
        if (owner) f();
    }
};

struct ScopeExitHelper {};
template <typename T>
ScopeExit<T> operator <<(ScopeExitHelper, T && f)
{
    return ScopeExit<T>(std::forward<T>(f));
}

#define TOKENPASTY(X, Y) X ## Y
#define TOKENPASTY2(X, Y) TOKENPASTY(X, Y)
#define defer auto const TOKENPASTY2(ScopeExit_invoker, __COUNTER__) = ScopeExitHelper() << [&]()


/// @brief read all from a file descriptor
///
/// @param fd file descriptor to read
/// @param f called back with partial contents
///
/// @throw system_error, see man 2 read
///
/// @code read_fd(fd, [&](char const *buffer, size_t size) {
///     ...
/// }
/// @endcode
template <typename F>
static inline void read_fd(int fd, F && f)
{
    char buffer[4096];
    ssize_t nread;
    
    for ( ; ; ) {
        while ((nread = ::read(fd, buffer, sizeof(buffer))) > 0) {
            f(buffer, (size_t)nread);
        }
        if (nread == 0)
            return;

        auto const error = error_code_from_errno();
        if (error != std::errc::interrupted)
            throw std::system_error(error, "read failed");
    }
}

/// @brief read all from a file descriptor into a std::string
static inline std::string read_fd(int fd)
{
    auto result = std::string();
    read_fd(fd, [&](char const *const buffer, size_t const count) {
        result.append(buffer, count);
    });
    return result;
}

/// @brief helper class for doing range-for on iterate-able things that don't have begin/end, like c arrays
///
/// @tparam ITER the type of iterator
template <typename ITER>
struct Sequence {
private:
    ITER const beg_;
    ITER const end_;

public:
    Sequence(ITER const &beg, ITER const &end) : beg_(beg), end_(end) {}

    ITER begin() const { return beg_; }
    ITER end() const { return end_; }
};

/// @brief helper function to allow type inference of iterator type
template <typename ITER>
static inline Sequence<ITER> make_sequence(ITER const &beg, ITER const &end) {
    return Sequence<ITER>(beg, end);
}

/// @brief helper function to allow type inference of iterator type
template <typename ITER>
static inline Sequence<ITER> make_sequence(ITER const &beg, size_t const count) {
    return Sequence<ITER>(beg, beg + count);
}

static inline bool pathExists(std::string const &path) {
    return access(path.c_str(), F_OK) == 0;
}

#if DEBUG || _DEBUGGING

#include <random>
static inline void randomfill(void *p, size_t size)
{
    auto begp = (char *)p;
    auto const endp = (char const *)(begp + size);
    std::random_device rdev;
    while (begp < endp) {
        auto const r = rdev();
        auto const end = (char const *)((&r) + 1);
        auto cur = (char const *)(&r);

        while (cur < end && begp < endp)
            *begp++ = *cur++;
    }
}

template <typename T, typename U>
static inline T *randomized(T *p, U const &init)
{
    randomfill(p, sizeof(T));
    return (new (p) T(init));
}

#endif
