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

#include <string>
#include <cassert>

#include "split_path.hpp"

#include "../../shared/toolkit.vers.h"

std::string split_basename(std::string *const path)
{
    auto result = std::string();
    auto i = path->begin();
    auto last = path->end();
    auto const end = last;
    
    while (i != end) {
        if (*i == '/') last = i;
        ++i;
    }
    if (last == end) {
        path->swap(result);
    }
    else {
        auto const save = last++;
        result = std::string(last, end);    // NB: might be empty
        path->erase(save, end);             // NB: might be whole string
    }
    return result;
}

std::string split_version(std::string *const name)
{
    auto result = std::string();
    auto i = name->begin();
    auto last = name->end();
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
    if (last == end) {
        result.reserve(14);
        result.assign(std::to_string((TOOLKIT_VERS) >> 24));
        result.append(1, '.');
        result.append(std::to_string(((TOOLKIT_VERS) >> 16) & 0xFF));
        result.append(1, '.');
        result.append(std::to_string((TOOLKIT_VERS) & 0xFFFF));
    }
    else {
        auto const save = last++;
        result.assign(last, end);
        name->erase(save, end);
    }
    return result;
}
