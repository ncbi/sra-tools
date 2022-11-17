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
 *  Declare and define compile-time constants (and some functions on them)
 *
 */

#pragma once

#include <klib/strings.h> /* SDL_CGI */

#include <cassert>
#include <algorithm>
#include <cstring>
#include <cstdlib>

#include "env_vars.h"
#include "util.hpp"

namespace constants {

/// @brief constants used for calling SRA Data Locator
struct resolver {
    static constexpr char const *version() { return "130"; }
#ifdef SDL_CGI
    static constexpr char const *url() { return SDL_CGI; }
#else
    static constexpr char const *url() { return "https://trace.ncbi.nlm.nih.gov/Traces/sdl/2/retrieve"; }
#endif

    /// @brief the current unstable version of SDL response JSON
    ///
    /// @Note THIS NEEDS TO TRACK ACTUAL SDL VALUE
    static constexpr char const *unstable_version() {
        return "2";
    }
};

/// @brief environment variables for passing information to the driven tool
struct env_var {
    /// @brief environment variables as symbolic names
    enum {
        CACHE_NEED_CE,
        CACHE_NEED_PMT,
        CACHE_URL,
        CACHE_VDBCACHE,
        CE_TOKEN,
        LOCAL_URL,
        LOCAL_VDBCACHE,
        REMOTE_NEED_CE,
        REMOTE_NEED_PMT,
        REMOTE_URL,
        REMOTE_VDBCACHE,
        SIZE_URL,
        SIZE_VDBCACHE,
        QUALITY_PREFERENCE,
        PARAMETER_BITS,
        END_ENUM
    };
    
    /// @brief array of environment variables in the same order as the enum above.
    static char const *const *names() {
        static char const *const value[] = {
            ENV_VAR_CACHE_NEED_CE,
            ENV_VAR_CACHE_NEED_PMT,
            ENV_VAR_CACHE_URL,
            ENV_VAR_CACHE_VDBCACHE,
            ENV_VAR_CE_TOKEN,
            ENV_VAR_LOCAL_URL,
            ENV_VAR_LOCAL_VDBCACHE,
            ENV_VAR_REMOTE_NEED_CE,
            ENV_VAR_REMOTE_NEED_PMT,
            ENV_VAR_REMOTE_URL,
            ENV_VAR_REMOTE_VDBCACHE,
            ENV_VAR_SIZE_URL,
            ENV_VAR_SIZE_VDBCACHE,
            ENV_VAR_QUALITY_PREFERENCE,
            ENV_VAR_PARAMETER_BITS,
        };
        return value;
    }

    static void preferNoQual(const char * v) {
        set(QUALITY_PREFERENCE, v);
    }
    
    /// @brief convert id to string
    ///
    /// @param iid integer id of env-var (range checked)
    ///
    /// @returns the env-var name as a string
    static char const *name(int const iid) {
        assert(0 <= iid && iid < END_ENUM);
        if (0 <= iid && iid < END_ENUM)
            return names()[iid];
        throw std::invalid_argument("unknown environment variable id");
    }
    
    /// @brief convert string to id
    ///
    /// @param qry the env-var name
    ///
    /// @returns the id or -1 if not found
    static int find(char const *qry) {
        auto const values = names();
        int f = 0;
        int e = END_ENUM;
        
        while (f < e) {
            auto const m = f + ((e - f) >> 1);
            auto const c = strcmp(values[m], qry);
            if (c < 0)
                f = m + 1;
            else if (c > 0)
                e = m;
            else
                return m;
        }
        return -1;
    }
    
    /// @brief unset environment variable by symbolic id
    ///
    /// @param iid the variable to unset
    static void unset(int const iid) {
#if WINDOWS
        std::string n = name(iid);
        n += "=";
        _putenv(n.c_str());
#else
        unsetenv(name(iid));
#endif
    }

    /// @brief set (or unset) environment variable by symbolic id
    ///
    /// @param iid the variable to set
    /// @param value the new value, unset if null
    /// @param overwrite overwrite the value if it is already there, default is to overwrite
    static void set(int const iid, char const *const value, bool const overwrite = true) {
        auto const envar = name(iid);
        EnvironmentVariables::set(envar, value ? EnvironmentVariables::Value(value) : EnvironmentVariables::Value());

        (void)(overwrite);
    }
};

}
