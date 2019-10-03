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

#if __cplusplus < 201703L
// std::optional is not available; cook one up
class opt_string {
    std::string maybe_value;
    bool is_set;
public:
    opt_string()
    : is_set(false)
    {
        
    }
    opt_string(std::string const &other)
    : is_set(true)
    , maybe_value(other)
    {
        
    }
    opt_string(opt_string const &other)
    : is_set(other.is_set)
    , maybe_value(other.maybe_value) // safe for std::string
    {
        
    }
    opt_string &operator =(opt_string const &other)
    {
        is_set = other.is_set;
        maybe_value = other.maybe_value;
        return *this;
    }
    bool has_value() const {
        return is_set;
    }
    operator bool() const {
        return has_value();
    }
    operator std::string() const {
        assert(is_set);
        return maybe_value;
    }
    std::string const &value() const {
        assert(is_set);
        return maybe_value;
    }
    std::string const &value_or(std::string const &alt) const {
        return is_set ? maybe_value : alt;
    }
};

#else // c++17 or higher
#include <optional>

using opt_string = std::optional<std::string>;
#endif // c++17
