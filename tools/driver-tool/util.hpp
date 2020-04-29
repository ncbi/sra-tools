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
#include <memory>
#include <stdlib.h>

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

static inline std::error_code error_code_from_errno();

static inline void throw_system_error [[noreturn]] (char const *const what)
{
    throw std::system_error(error_code_from_errno(), what);
}

static inline void throw_system_error [[noreturn]] (std::string const &what)
{
    throw std::system_error(error_code_from_errno(), what);
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

#endif // DEBUG || _DEBUGGING

#if WINDOWS
#include "util.win32.hpp"
#else
#include "util.posix.hpp"
#endif
