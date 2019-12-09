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
#include <cassert>
#include <algorithm>
#include <cstring>
#include <cstdlib>

#include "env_vars.h"

namespace constants {

/// @brief constants used for tool names
struct tool_name {
    /// @brief tools names as symbols
    enum {
        FASTERQ_DUMP,
        FASTQ_DUMP,
        PREFETCH,
        SAM_DUMP,
        SRA_PILEUP,
        SRAPATH,
        END_ENUM
    };
    
    /// @brief array of tool names in same order as above enum
    static char const *const *real() {
        static char const *const value[] = {
            "fasterq-dump-orig",
            "fastq-dump-orig",
            "prefetch-orig",
            "sam-dump-orig",
            "sra-pileup-orig",
            "srapath-orig"
        };
        return value;
    }
    
    /// @brief array of impersonated tool names in same order as above enum
    static char const *const *runasNames() {
        static char const *const value[] = {
            "fasterq-dump",
            "fastq-dump",
            "prefetch",
            "sam-dump",
            "sra-pileup",
            "srapath"
        };
        return value;
    }
    
    /// @brief get full path to tool by id
    static char const *path(int const iid)
    {
        extern std::vector<opt_string> load_tool_paths(int n, char const *const *runas, char const *const *real);
        extern void pathHelp [[noreturn]] (std::string const &toolname);

        static auto const cache = load_tool_paths(END_ENUM, runasNames(), real());
        auto const &result = cache.at(iid);
        if (result)
            return result.value().c_str();
        
        pathHelp(runas(iid));
    }
    
    /// @brief convert id to string
    ///
    /// @param iid integer id of tool (range checked)
    ///
    /// @returns the real name of the tool in the filesystem
    static char const *real(int const iid) {
        assert(0 <= iid && iid < END_ENUM);
        if (0 <= iid && iid < END_ENUM)
            return real()[iid];
        throw std::range_error("unknown tool id");
    }
    
    /// @brief convert id to string
    ///
    /// @param iid integer id of tool (range checked)
    ///
    /// @returns the impersonated name of the tool
    static char const *runas(int const iid) {
        assert(0 <= iid && iid < END_ENUM);
        if (0 <= iid && iid < END_ENUM)
            return runasNames()[iid];
        throw std::range_error("unknown tool id");
    }
    
    /// @brief convert impersonated name to id
    ///
    /// @param qry the impersonated name
    ///
    /// @returns the id or -1 if not found
    static int lookup_iid(char const *const qry) {
        auto const values = runasNames();
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
};

/// @brief constants used for calling SRA Data Locator
struct resolver {
    static constexpr char const *version() { return "130"; }
    static constexpr char const *url() { return "https://trace.ncbi.nlm.nih.gov/Traces/sdl/2/retrieve"; }

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
            ENV_VAR_SIZE_VDBCACHE
        };
        return value;
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
        throw std::range_error("unknown environment variable id");
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
        unsetenv(name(iid));
    }

    /// @brief set (or unset) environment variable by symbolic id
    ///
    /// @param iid the variable to set
    /// @param value the new value, unset if null
    /// @param overwrite overwrite the value if it is already there, default is to overwrite
    static void set(int const iid, char const *value, bool overwrite = true) {
        auto const envar = name(iid);
        if (value)
            setenv(envar, value, overwrite ? 1 : 0);
        else
            unsetenv(envar);
    }
};

}
