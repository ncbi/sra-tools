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
 *  uuid
 *
 */

#include <string>
#include <random>
#include <limits>
#include <cstring>
#include <cassert>

#include "uuid.hpp"

#ifndef TESTING
static
#endif
/// @brief generate a type 4 version 1 uuid
///
/// @param buffer filled in with new uuid
void uuid_random(char buffer[36])
{
    static constexpr char const tr[] = "0123456789abcdef";
    // xxxxxxxx-xxxx-4xxx-Nxxx-xxxxxxxxxxxx where x is a random hex digit
    int cur = 0;
    auto dist = std::uniform_int_distribution<uint32_t>(0, UINT32_MAX);
    std::random_device rd;

    for (auto i = 0; i < 4 && cur < 36; ++i) {
        auto r = dist(rd);
        for (auto j = 0; j < 8 && cur < 36; ) {
            switch (cur) {
            default:
                buffer[cur++] = tr[r & 0x0F];
                r >>= 4;
                ++j;
                break;
            case 8:
            case 8+5:
            case 8+5+5:
            case 8+5+5+5:
                buffer[cur++] = '-';
                break;
            case 8+5+1:
                buffer[cur++] = '4';
                break;
            case 8+5+5+1:
                buffer[cur++] = tr[0x8 | (r & 0x3)];
                r >>= 4;
                ++j;
                break;
            }
        }
    }
}

std::string uuid()
{
    char buffer[36];
    uuid_random(buffer);
    return std::string(buffer, buffer + 36);
}
