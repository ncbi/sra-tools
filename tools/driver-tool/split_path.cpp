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
 *  argv[0] manipulations
 *
 */

#pragma once
#include <string>
#include <cctype>
#include <iterator>

/// @brief: split path into dirname and basename
///
/// @param dirname: [inout] directory name part
///
/// @returns basename part
std::string split_basename(std::string &path)
{
    auto result = std::string();
    auto i = path.begin();
    auto last = path.end();
    auto const end = last;
    
    while (i != end) {
        if (*i == '/') last = i;
        ++i;
    }
    if (last == end) {
        path.swap(result);
    }
    else {
        ++last;
        result = std::string(last, end);    // NB: might be empty
        path.erase(last, end);              // NB: might be whole string
    }
    return result;
}

/// @brief: split basename into name and version
///
/// @param name: [inout] the basename
/// @param default_version: version to use if there is no version part
///
/// @returns version part
std::string split_version(std::string &name, std::string const &default_version)
{
    auto i = name.begin();
    auto last = name.end();
    auto const end = last;
    
    while (i != end) {
        if (last == end) {
            if (*i == '.')
                last = i;
        }
        else if (!isdigit(*i) && *i != '.')
            last = end;
        ++i;
    }
    auto result = last == end ? default_version : std::string(std::next(last), end);
    name.erase(last, end);
    return result;
}
