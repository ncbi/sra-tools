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
 *  uuid testing
 *
 */

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cassert>
#include "uuid.hpp"

/// @brief validate type 4 version 1 UUID
///
/// verifies length, field content, and divider placement
///
/// @param s the UUID to validate
///
/// @returns 0 if good, else offset of first error
static int validate_uuid_format(char const s[])
{
    {
        auto n = 0;
        auto field = 1;
        auto length = 0;
        for ( ; ; ) {
            auto const ch = s[n++];
            if (ch == '\0') {
                --n;
                break;
            }
            ++length;
            if (ch == '-') {
                --length;
                switch (field) {
                    case 1:
                        if (length != 8)
                            return n;
                        break;
                    case 2:
                    case 3:
                    case 4:
                        if (length != 4)
                            return n;
                        break;
                    default:
                        return n;
                }
                ++field;
                length = 0;
            }
            else if (!std::isxdigit(ch))
                return n;
        }
        if (n != 36)
            return n;
    }
    {
        auto const &type = s[14];
        auto const &vers_char = s[19];
        auto const vers = (vers_char <= '9' ? (vers_char - '0') : (10 + vers_char - 'a'));
        if (type != '4') {
            return 15;
        }
        if ((vers & 0xC) != 0x8) {
            return 20;
        }
    }
    return 0;
}

/// @brief verify that validation function does not reject good UUIDs
static void test_success()
{
    assert(validate_uuid_format("a38fa5da-1456-498a-9ef5-1f39e35bfe57") == 0);
}

/// @brief verify that validation function does reject bad UUIDs
static void test_failure()
{
    assert(validate_uuid_format("a38fa5da-1456-498a-9ef5-1f39e35b") != 0);
    assert(validate_uuid_format("a38fa5da-1456-498a-9ef5-1f39e35bfe57fe57") != 0);
    assert(validate_uuid_format("a38fa5da-1456-498a-Nef5-1f39e35bfe57") != 0);
    assert(validate_uuid_format("a38fa5da-1456-498a-9ef5-1f39-35bfe57") != 0);
    assert(validate_uuid_format("xxxxxxxx-xxxx-4xxx-Nxxx-xxxxxxxxxxxx") != 0);
}

int main(int argc, const char * argv[]) {
    test_success();
    test_failure();
    
    for (auto i = 0; i < 1000; ++i) {
        char buffer[37];

        std::memset(buffer, 0, 37);
        uuid_random(buffer);

        assert(validate_uuid_format(buffer) == 0);
    }
    return 0;
}
