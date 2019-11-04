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
#include "uuid.hpp"

template <int COUNT, int BITS>
class RandomValues
{
public:
    using RandomValue = std::random_device::result_type;
    using RandomValueType = std::numeric_limits<RandomValue>;
    static_assert(RandomValueType::digits >= BITS && RandomValueType::is_signed == false, "!!!");
    
    static auto constexpr bits = RandomValueType::digits;
    auto static constexpr count = (BITS * COUNT + bits - 1) / bits;

    RandomValues() {
        std::random_device devRandom;
        for (auto i = 0; i < count; ++i)
            value[i] = devRandom();
    }
    RandomValue next() {
        if (b == 0) {
            i += 1;
            b = bits;
        }
        assert(i < count);
        auto const result = value[i] & ((1u << BITS) - 1u);
        value[i] >>= BITS;
        b -= BITS;
        return result;
    }
private:
    unsigned i = 0, b = bits;
    RandomValue value[count];
};

#ifndef TESTING
static
#endif
/// @brief generate a type 4 version 1 uuid
///
/// @param buffer filled in with new uuid
void uuid_random(char buffer[37])
{
    auto rnd = RandomValues<31, 4>();
    static char const *hexdigits = "0123456789abcdef";
    
    for (auto i = 0; i < 36; ++i) {
        switch ("xxxxxxxx-xxxx-4xxx-Nxxx-xxxxxxxxxxxx"[i]) {
                // x <= a random lowercase hex digit
                // N <= a random character from [89ab]
        case 'x':
            buffer[i] = hexdigits[rnd.next()];
            break;
        case '-':
            buffer[i] = '-';
            break;
        case '4':
            buffer[i] = '4';
            break;
        case 'N':
            buffer[i] = hexdigits[(rnd.next() & 0x03) | 0x08]; // take only the 2 low order bits and set the 2 high order bits to 10
            break;
        }
    }
    buffer[36] = '\0';
}

std::string uuid()
{
    char buffer[37];
    uuid_random(buffer);
    return std::string(buffer, buffer + 36);
}
