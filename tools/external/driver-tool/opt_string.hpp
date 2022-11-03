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
 *  opt_string definition
 *
 */

#pragma once

#include <string>

#if __cplusplus < 201703L || !defined(__cpp_lib_optional)
#include <cassert>
#include <type_traits>
#include <utility>

/**
 @brief minimally compatible with std::optional
 */
class opt_string {
    /// @brief the value, if one was set
    std::string maybe_value;

    /// @brief true if a value was set
    bool is_set = false;
public:
    /// @brief without the value set
    opt_string() = default;
    
    /// @brief copied
    opt_string(opt_string const &other) = default;
    opt_string(opt_string &&other) = default;

    /// @brief assigned
    opt_string &operator =(opt_string const &other) = default;
    opt_string &operator =(opt_string &&other) = default;

    /// @brief with the value set
    explicit opt_string(char const *other)
    : is_set(other != nullptr)
    {
        if (is_set)
            maybe_value.assign(other);
    }

    /// @brief with the value set
    explicit opt_string(std::string const &other)
    : maybe_value(other)
    , is_set(true)
    {}

    /// @brief with the value set
    explicit opt_string(std::string &&other)
    : maybe_value(std::move(other))
    , is_set(true)
    {}

    opt_string &operator= (std::string const &other)
    {
        is_set = true;
        maybe_value = other;
        return *this;
    }

    opt_string &operator= (std::string &&other)
    {
        is_set = true;
        maybe_value = std::move(other);
        return *this;
    }
    
    opt_string &operator= (char const *other)
    {
        is_set = other != nullptr;
        if (is_set)
            maybe_value.assign(other);
        else
            maybe_value.clear();
        return *this;
    }

    /// @brief is the value set
    bool has_value() const {
        return is_set;
    }

    /// @brief is the value set
    operator bool() const {
        return has_value();
    }

    /// @brief get the value; undefined if no value was set
    operator std::string() const {
        assert(is_set);
        return maybe_value;
    }

    /// @brief get the value; undefined if no value was set
    std::string &value() {
        assert(is_set);
        return maybe_value;
    }

    /// @brief get the value; undefined if no value was set
    std::string const &value() const {
        assert(is_set);
        return maybe_value;
    }

    /// @brief get the value or the alternate value
    /// @param alt the alternate value if not set
    std::string const &value_or(std::string const &alt) const {
        return is_set ? maybe_value : alt;
    }
    
    std::string &operator* () { return value(); }
    std::string const &operator* () const { return value(); }
};

#else // c++17 or higher
#include <optional>

using opt_string = std::optional<std::string>;
#endif // c++17
