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
#include <cstdio>
#include <ctime>
#include <cassert>

#include "uuid.hpp"

#ifndef TESTING
static
#endif
/// @brief generate a type 4 version 1 uuid
///
/// @param buffer filled in with new uuid
void uuid_random(char buffer[37])
{
    std::random_device s; // seeder
    auto const s1 = s();
    auto const s2 = time(nullptr);
    std::mt19937 gen1(s1 ^ decltype(s1)(s2 >> 32)), gen2(s1 ^ decltype(s1)(s2));
    std::uniform_int_distribution<uint16_t> d16;
    std::uniform_int_distribution<uint32_t> d32;

    // xxxxxxxx-xxxx-4xxx-Nxxx-xxxxxxxxxxxx
    //    08x    04x  04x  04x  04x   08x
    sprintf(buffer, "%08x-%04x-%04x-%04x-%04x%08x"
            , d32(gen1), d16(gen2)
            , (d16(gen2) & 0x0FFF) | 0x4000 ///< type 4 (4 in high nibble)
            , (d16(gen2) & 0x3FFF) | 0x8000 ///< version 1 (highest 2 bits set to 10)
            , d16(gen2), d32(gen1));
}

std::string uuid()
{
    char buffer[37];
    uuid_random(buffer);
    return std::string(buffer, buffer + 36);
}
