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

#include <windows.h>
#include <stringapiset.h>
#include <memory.h>
#include <limits.h>

#include <map>
#include <cassert>

typedef SSIZE_T ssize_t;

struct Win32Shim {
    /// Convert UTF8 string to Windows wide char string
    /// Note: if len == -1, the returned count includes the nil-terminator
    /// @param wstr buffer to recieve converted string
    /// @param wlen max number of wide chars that wstr can hold, NB. not size in bytes
    /// @param str the string to convert
    /// @param len the length of str or -1 if nil-terminated
    /// @return number of wide chars placed into wstr (including any nil-terminator)
    static ssize_t widen(wchar_t *wstr, size_t wlen, char const *str, ssize_t len = -((ssize_t)1))
    {
        assert((0 <= len && len <= INT_MAX) || len == -((ssize_t)1));
        assert(0 <= wlen && wlen <= INT_MAX);
        return (ssize_t)MultiByteToWideChar(CP_UTF8, 0, str, (int)len, wstr, (int)wlen);
    }

    /// Get the number of wide chars needed to hold a converted string
    /// Note: if len == -1, the returned count includes the nil-terminator
    /// @param str the string to measure
    /// @param len the length of str or -1 if nil-terminated
    /// @return number of wide chars needed to hold converted str
    static ssize_t wideSize(char const *str, ssize_t len = -((ssize_t)1))
    {
        assert((0 <= len && len <= INT_MAX) || len == -((ssize_t)1));
        wchar_t wdummy[1];
        return (ssize_t)widen(wdummy, 0, str, len);
    }

    /// Convert nil-terminated UTF8 string to Windows nil-terminated wide char string
    /// @param str the string to convert
    /// @return a new wide char string, deallocate with free
    static wchar_t *makeWide(char const *str)
    {
        auto const wlen = wideSize(str);
        if (wlen > 0) {
            auto const wvalue = (wchar_t *)malloc(wlen * sizeof(wchar_t));
            if (wvalue != NULL) {
                widen(wvalue, wlen, str);
                return wvalue;
            }
        }
        return NULL;
    }

    /// Convert Windows wide char string to a UTF8 string
    /// Note: if len == -1, the returned count includes the nil-terminator
    /// @param str buffer to recieve converted string
    /// @param len max number of chars that str can hold
    /// @param wstr the string to convert
    /// @param wlen the length of wstr or -1 if nil-terminated
    /// @return number of chars placed into str (including any nil-terminator)
    static ssize_t unwiden(char *str, ssize_t len, wchar_t const *wstr, ssize_t wlen = -((ssize_t)1))
    {
        assert(0 <= len && len <= INT_MAX);
        assert((0 <= wlen && wlen <= INT_MAX) || wlen == -((ssize_t)1));
        return (ssize_t)WideCharToMultiByte(CP_UTF8, 0, wstr, (int)wlen, str, (int)len, NULL, NULL);
    }

    /// Get the number of chars needed to hold a converted wide string
    /// Note: if srclen == -1, the returned count includes the nil-terminator
    /// @param wstr the string to measure
    /// @param wlen the length of wstr or -1 if nil-terminated
    /// @return number of chars needed to hold converted wstr
    static ssize_t unwideSize(wchar_t const *wstr, ssize_t wlen = -((ssize_t)1))
    {
        assert((0 <= wlen && wlen <= INT_MAX) || wlen == -((ssize_t)1));
        char dummy[1];
        return (ssize_t)unwiden(dummy, 0, wstr, wlen);
    }

    /// Convert Windows nil-terminated wide char string to nil-terminated UTF8 string
    /// @param wvalue the wide string to convert
    /// @return a new char string, deallocate with free
    static char *makeUnwide(wchar_t const *wvalue)
    {
        auto const len = unwideSize(wvalue);
        if (len > 0) {
            auto const value = (char *)malloc(len * sizeof(char));
            if (value != NULL) {
                unwiden(value, len, wvalue);
                return value;
            }
        }
        return NULL;
    }
};

static char *GetFullPathToExe()
{
    for (DWORD size = 4096; ; size *= 2) {
        auto const wbuffer = std::unique_ptr<wchar_t *, decltype(free)>((wchar_t *)malloc(((size_t)size) * sizeof(wchar_t)), free);
        if (!wbuffer)
            return NULL;
        if (GetModuleFileNameW(NULL, wbuffer.get(), (DWORD)size) < size)
            return Win32Shim::makeUnwide(wbuffer);
    }
}

static bool pathExists(std::string const &path) {
    auto const wpath = std::unique_ptr<wchar_t *, decltype(free)>(makeWide(path.c_str()), free);
    assert(wpath);
    auto const attr = GetFileAttributesW(wpath);
    return attr != INVALID_FILE_ATTRIBUTES;
}

class EnvironmentVariables {
public:
    class Value : public std::string {
        bool notSet;
    public:
        Value(std::string const &value) : notSet(false), std::string(value) {}
        Value() : notSet(true) {}
        operator bool() const { return !notSet; }
        bool operator !() const { return notSet; }
    };
    using Set = std::map<std::string, Value>;

    static Value get(std::string const &name) {
        wchar_t wdummy[4];
        auto const wname = std::unique_ptr<wchar_t *, decltype(free)>(makeWide(name.c_str()), free);
        assert(wname);

        auto const wvaluelen = GetEnvironmentVariableW(wname.get(), wdummy, 0);
        if (wvaluelen > 0) {
            auto const wbuffer = std::unique_ptr<wchar_t *, decltype(free)>((wchar_t *)malloc((wvaluelen + 1) * sizeof(wchar_t)), free);
            assert(wbuffer);

            GetEnvironmentVariableW(wname.get(), wbuffer, 0);

            auto const value = std::unique_ptr<char *, decltype(free)>(makeUnwide(wbuffer), free);
            assert(value);
            return Value(std::string(value));
        }
        return Value();
    }
    static void set(std::string const &name, Value const &value) {
        auto const wname = std::unique_ptr<wchar_t *, decltype(free)>(makeWide(name.c_str()), free);
        assert(wname);

        if (value) {
            auto const wvalue = std::unique_ptr<wchar_t *, decltype(free)>(makeWide(value.c_str()), free);
            assert(wvalue);

            SetEnvironmentVariableW(wname, wvalue);
        }
        else {
            SetEnvironmentVariableW(wname, NULL);
        }
    }
    static Set set_with_restore(std::map<std::string, std::string> const &vars) {
        auto result = Set();
        for (auto && v : vars) {
            result[v.first] = get(v.first);
            set(v.first, v.second.empty() ? Value() : v.second);
        }
        return result;
    }
    static void restore(Set const &save) {
        for (auto && v : save) {
            set(v.first, v.second);
        }
    }
    static char const *impersonate() {
        return NULL;
    }
};
