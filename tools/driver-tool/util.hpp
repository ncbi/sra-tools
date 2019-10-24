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
#include <string>
#include <unistd.h>

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

/// @brief read all from a file descriptor
///
/// @param fd file descriptor to read
/// @param f gets called back with partial contents (char const *, size_t) -> void
///
/// @throw system_error, see man 2 read
template <typename F>
void read_fd(int fd, F && f)
{
    char buffer[4096];
    ssize_t nread;
    
    for ( ; ; ) {
        while ((nread = ::read(fd, buffer, sizeof(buffer))) > 0) {
            f(buffer, nread);
        }
        if (nread == 0)
            return;

        auto const error = error_code_from_errno();
        if (error != std::errc::interrupted)
            throw std::system_error(error, "read failed");
    }
}
