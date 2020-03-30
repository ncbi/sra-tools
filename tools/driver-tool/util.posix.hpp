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

#include <string>
#include <map>
#include <unistd.h>

static inline bool pathExists(std::string const &path) {
    return access(path.c_str(), F_OK) == 0;
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
        auto const value = getenv(name.c_str());
        if (value)
            return Value(std::string(value));
        return Value();
    }
    static void set(std::string const &name, Value const &value) {
        if (value)
            setenv(name.c_str(), value.c_str(), 1);
        else
            unsetenv(name.c_str());
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
        return getenv("SRATOOLS_IMPERSONATE");
    }
};
