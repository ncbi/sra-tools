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

#if WINDOWS

#if _MSC_VER
#define MS_Visual_C 1
#endif

#pragma warning(disable: 4668)

#if __INTELLISENSE__
#define MS_Visual_C 1
#endif

#define NOMINMAX 1

#if MS_Visual_C
// Need to turn off all sorts of useless noise
#pragma warning(disable: 5039)
#pragma warning(disable: 4267)
#pragma warning(disable: 4365)
#pragma warning(disable: 4458)
#pragma warning(disable: 4514)
#pragma warning(disable: 4505)
#pragma warning(disable: 5245)
#pragma warning(disable: 5045)
#pragma warning(disable: 4820) // REALLY!? You are going to complain about having to do aligning! Like the standard requires!? Lazy garbage! So typical
#pragma warning(disable: 4464)
#endif

#include <Windows.h>
#include <stringapiset.h>
#include <Fileapi.h>
#include <Shlwapi.h>
#include <pathcch.h>
#include <errhandlingapi.h>
#include <WinNls.h>

#include <memory.h>
#include <limits.h>
#include <map>
#include <cassert>
#include <system_error>

#include "wide-char.hpp"
#include "opt_string.hpp"

static inline std::error_code error_code_from_errno()
{
    return std::error_code((int)GetLastError(), std::system_category());
}

namespace Win32 {
struct EnvironmentVariables {
    static opt_string get(char const *name) {
        opt_string result;
#if USE_WIDE_API
        wchar_t wdummy[4];
        auto const wname = Win32Support::makeWide(name);
        assert(wname);

        auto const wvaluelen = GetEnvironmentVariableW(wname.get(), wdummy, 0);
        if (wvaluelen > 0) {
            auto const wbuffer = Win32Support::auto_free_wide_ptr((wchar_t *)malloc(sizeof(wchar_t) * wvaluelen));
            if (!wbuffer)
                throw std::bad_alloc();
            GetEnvironmentVariableW(wname.get(), wbuffer.get(), wvaluelen);

            result = Win32Support::makeUnwideString(wbuffer.get());
        }
#else
        char dummy[4];
        auto const valuelen = GetEnvironmentVariableA(name, dummy, 0);
        if (valuelen > 0) {
            auto const buffer = Win32Support::auto_free_char_ptr((char *)malloc(sizeof(char) * valuelen));
            if (!buffer)
                throw std::bad_alloc();
            GetEnvironmentVariableA(name, buffer.get(), valuelen + 1);
            result = std::string(buffer.get());
        }
#endif
        return result;
    }
    static void set(char const *name, char const *value) {
#if USE_WIDE_API
        auto const wname = Win32Support::makeWide(name);
        if (value) {
            auto const wvalue = Win32Support::makeWide(value);
            SetEnvironmentVariableW(wname.get(), wvalue.get());
        }
        else
            SetEnvironmentVariableW(wname.get(), NULL);
#else
        SetEnvironmentVariableA(name, value);
#endif
    }
};
}

using PlatformEnvironmentVariables = Win32::EnvironmentVariables;

#endif
