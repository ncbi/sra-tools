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
 *  provide support for Windows wide-char API
 *
 */

#pragma once

#ifndef _H_WIDE_CHAR_HPP_
#define _H_WIDE_CHAR_HPP_ 1

#include <memory>           // unique_ptr
#include <cstdlib>          // malloc, free
#include <new>              // std::bad_alloc
#include <stdexcept>        // std::invalid_argument
#include <string>           // std::string

#if WINDOWS

#include <Windows.h>
#include <errhandlingapi.h> // GetLastError
#include <stringapiset.h>   // MultiByteToWideChar, WideCharToMultiByte
#include <WinNls.h>
using ssize_t = SSIZE_T;

#else
// To make the compiler happy
typedef unsigned int UINT;
typedef unsigned long DWORD;
#define CP_UTF8 0
#define WC_ERR_INVALID_CHARS 0
#define MB_ERR_INVALID_CHARS 0
#define ERROR_NO_UNICODE_TRANSLATION 1
#define ERROR_INVALID_FLAGS 2

static int GetLastError() { return 0; }
static int MultiByteToWideChar(UINT cp, DWORD flags, char const *in, int inlen, wchar_t *out, int outlen)
{
    return inlen;
}
static int WideCharToMultiByte(UINT cp, DWORD flags, wchar_t const *in, int inlen, char *out, int outlen, char const *ignore, bool *notused)
{
    return inlen;
}
#endif

namespace Win32Support {
    namespace impl {
        namespace {
            struct auto_free {
                template < typename T >
                void operator ()(T *ptr) const {
                    std::free(reinterpret_cast< void * >(ptr));
                }
            };

            template < typename T >
            using auto_free_ptr = std::unique_ptr< T, auto_free >;

            static inline void throw_error [[noreturn]] (std::string function)
            {
                switch (GetLastError()) {
                case ERROR_NO_UNICODE_TRANSLATION:
                    throw std::invalid_argument(function + ": unconvertable string");
                case ERROR_INVALID_FLAGS:
                    throw std::invalid_argument(function + ": invalid flags");
                default:
                    throw std::invalid_argument(function + ": invalid parameter");
                }
            }
        }
    }
    using impl::auto_free_ptr;
    using auto_free_char_ptr = impl::auto_free_ptr<char>;
    using auto_free_wide_ptr = impl::auto_free_ptr<wchar_t>;

    /// Convert UTF-8 string to Windows wide char string
    /// Note: if len == -1, the returned count includes the nil-terminator
    /// @param wstr buffer to recieve converted string
    /// @param wlen max number of wide chars that wstr can hold, NB. not size in bytes
    /// @param str the string to convert
    /// @param len the length of str or -1 if nil-terminated
    /// @return number of wide chars placed into wstr (including any nil-terminator)
    static inline size_t widen(wchar_t *wstr, size_t wlen, char const *str, ssize_t len = -((ssize_t)1))
    {
        auto const length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, (int)len, wstr, (int)wlen);
        if (length > 0)
            return (size_t)length;
        impl::throw_error("MultiByteToWideChar");
    }

    /// Get the number of wide char code units needed to hold a converted string
    /// Note: if len == -1, the returned count includes the nil-terminator
    /// @param str the string to measure
    /// @param len the length of str or -1 if nil-terminated
    /// @return number of wide char code units needed to hold converted str
    static inline size_t wideSize(char const *str, ssize_t len = -((ssize_t)1))
    {
        wchar_t dummy[1];
        return Win32Support::widen(dummy, 0, str, len);
    }

    namespace impl { namespace {
        /// Convert nil-terminated UTF-8 string to Windows nil-terminated wide char string
        /// @param str the string to convert
        /// @return a new wide char string, free it.
        static inline wchar_t *makeWide(char const *str)
        {
            auto const len = Win32Support::wideSize(str);
            auto result = (wchar_t *)malloc(len * sizeof(wchar_t));
            if (result != nullptr) {
                Win32Support::widen(result, len, str);
                return result;
            }
            throw std::bad_alloc();
        }
    }}

    /// Convert nil-terminated UTF-8 string to Windows nil-terminated wide char string
    /// @param str the string to convert.
    /// @return a new wide char string, auto frees.
    static inline Win32Support::auto_free_wide_ptr makeWide(char const *str)
    {
        return Win32Support::auto_free_wide_ptr(impl::makeWide(str));
    }

    /// Convert nil-terminated UTF-8 string to Windows nil-terminated wide char string
    /// @param str wrapper for the string to convert.
    /// @return a new wide char string, auto frees.
    static inline Win32Support::auto_free_wide_ptr makeWide(auto_free_char_ptr const &str)
    {
        return Win32Support::makeWide(str.get());
    }

    /// Convert Windows wide char string to a UTF-8 string
    /// Note: if len == -1, the returned count includes the nil-terminator
    /// @param str buffer to recieve converted string
    /// @param len max number of chars that str can hold
    /// @param wstr the string to convert
    /// @param wlen the length of wstr or -1 if nil-terminated
    /// @return number of chars placed into str (including any nil-terminator)
    static inline size_t unwiden(char *str, ssize_t len, wchar_t const *wstr, ssize_t wlen = -((ssize_t)1))
    {
        assert(0 <= len && len <= INT_MAX);
        assert(wlen == -((ssize_t)1) || (0 <= wlen && wlen <= INT_MAX));
        auto const length = WideCharToMultiByte(CP_UTF8, 0 /*WC_ERR_INVALID_CHARS*/, wstr, (int)wlen, str, (int)len, NULL, NULL);
        if (length > 0)
            return (size_t)length;
        impl::throw_error("WideCharToMultiByte");
    }

    /// Get the number of UTF-8 code units needed to hold a converted wide string
    /// Note: if srclen == -1, the returned count includes the nil-terminator
    /// @param str the string to measure
    /// @param len the length of str or -1 if nil-terminated
    /// @return number of UTF-8 code units needed to hold converted string
    static inline size_t unwideSize(wchar_t const *str, ssize_t len = -((ssize_t)1))
    {
        char dummy[1];
        return Win32Support::unwiden(dummy, 0, str, len);
    }

    namespace impl { namespace {
        /// Convert Windows nil-terminated wide char string to nil-terminated UTF-8 string
        /// @param str the wide string to convert
        /// @return a new char string, free it.
        static inline char *makeUnwide(wchar_t const *str)
        {
            auto const len = Win32Support::unwideSize(str);
            auto result = (char *)malloc(len * sizeof(char));
            if (result != nullptr) {
                Win32Support::unwiden(result, (ssize_t)len, str);
                return result;
            }
            throw std::bad_alloc();
        }
    }}

    /// Convert Windows nil-terminated wide char string to nil-terminated UTF-8 string
    /// @param str the wide string to convert
    /// @return a new char string, auto frees.
    static inline Win32Support::auto_free_char_ptr makeUnwide(wchar_t const *str)
    {
        return Win32Support::auto_free_char_ptr(impl::makeUnwide(str));
    }

    /// Convert Windows nil-terminated wide char string to nil-terminated UTF-8 string
    /// @param str wrapper for the wide string to convert
    /// @return a new char string, auto frees.
    static inline Win32Support::auto_free_char_ptr makeUnwide(Win32Support::auto_free_wide_ptr const &str)
    {
        return Win32Support::makeUnwide(str.get());
    }

    /// Convert Windows nil-terminated wide char string to c++ UTF-8 string
    /// @param str wrapper for the wide string to convert
    /// @return a std::string.
    static inline std::string makeUnwideString(wchar_t const *str)
    {
        auto const narrow = impl::makeUnwide(str);
        auto result = std::string(narrow);
        free(narrow);
        return result;
    }
} // namespace Win32Support

#endif /* _H_WIDE_CHAR_HPP_ */
