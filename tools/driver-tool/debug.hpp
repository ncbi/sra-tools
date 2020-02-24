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
 *  Declare and define debugging facilities
 *
 */

#pragma once
#include <iostream>
#include <cassert>
#include <cstdlib>

/// @brief translate environment variables for controlling debugging features
struct logging_state {
    static bool is_debug() {
        static auto const value(get_debug_value());
        return value;
    }
    static bool is_trace() {
        static auto const value(get_trace_value());
        return value;
    }
    static int verbosity() {
        static auto const value(get_verbose_value());
        return value;
    }
    static bool is_verbose(int const level) {
        return level <= verbosity();
    }
    static int testing_level() {
        static auto const value(get_testing_value());
        return value;
    }
    static bool is_dry_run() {
        auto const level = testing_level();
        return 2 <= level && level <= 4;
    }
private:
    logging_state() = delete;
    static bool is_falsy(char const *const str) {
        return (str == NULL || str[0] == '\0' || (str[0] == '0' && str[1] == '\0'));
    }
    static bool get_debug_value() {
        return !is_falsy(getenv("SRATOOLS_DEBUG"));
    }
    static bool get_trace_value() {
        return !is_falsy(getenv("SRATOOLS_TRACE"));
    }
    static int get_verbose_value() {
        auto const str = getenv("SRATOOLS_VERBOSE");
        if (str && str[0] && str[1] == '\0') {
            return std::atoi(str);
        }
        return 0;
    }
    static bool get_dry_run() {
        return !is_falsy(getenv("SRATOOLS_DRY_RUN"));
    }
    static int get_testing_value() {
        auto const str = getenv("SRATOOLS_TESTING");
        if (str && str[0]) {
            return std::atoi(str);
        }
        return get_dry_run() ? 3 : 0;
    }
};

#define TRACE(X) do { if (logging_state::is_trace()) { std::cerr << "TRACE: " << __FILE__ << ':' << __LINE__ << " - " << __FUNCTION__ << ": " << #X << " = \n" << (X) << std::endl; } } while(0)

#define DEBUG_OUT if (!logging_state::is_debug()) {} else std::cerr

#define LOG(LEVEL) if (!logging_state::is_verbose(LEVEL)) {} else std::cerr
