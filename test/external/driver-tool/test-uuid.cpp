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
 *  Unit tests for random UUID generation
 *
 */

#if WINDOWS
#pragma warning(disable: 4100)
#pragma warning(disable: 4101)
#endif

#include <iostream>
#include <string>
#include <cassert>
#include "uuid.cpp"

static bool uuid_is_valid(std::string const &buffer)
{
    if (buffer.size() != 36) return false;

    for (auto i = 0; i < 36; ++i) {
        auto const ch = buffer[i];

        // 0         1         2         3     36
        // 012345678901234567890123456789012345
        // xxxxxxxx-xxxx-4xxx-Nxxx-xxxxxxxxxxxx
        switch (i) {
        case 8:
        case 13:
        case 18:
        case 23:
            if (ch != '-') return false;
            break;
        case 14:
            if (ch != '4') return false;
            break;
        case 19:
            switch (ch) {
            case '8':
            case '9':
            case 'a':
            case 'b':
                break;
            default:
                return false;
            }
            break;
        default:
            if (!isxdigit(ch)) return false;
            break;
        }
    }
    return true;
}

void uuid_test(void)
{
    assert(uuid_is_valid("a38fa5da-1456-498a-8ef5-1f39e35bfe57"));
    assert(uuid_is_valid("a38fa5da-1456-498a-9ef5-1f39e35bfe57"));
    assert(uuid_is_valid("a38fa5da-1456-498a-aef5-1f39e35bfe57"));
    assert(uuid_is_valid("a38fa5da-1456-498a-bef5-1f39e35bfe57"));
    assert(uuid_is_valid("a38fa5da-1456-098a-9ef5-1f39e35bfe57") == false); // invalid type
    assert(uuid_is_valid("a38fa5da-1456-498a-0ef5-1f39e35bfe57") == false); // invalid subtype
    assert(uuid_is_valid("a38fa5da11456-498a-9ef5-1f39e35bfe57") == false); // invalid seperator
    assert(uuid_is_valid("a38fa5da-14562498a-9ef5-1f39e35bfe57") == false); // invalid seperator
    assert(uuid_is_valid("a38fa5da-1456-498a39ef5-1f39e35bfe57") == false); // invalid seperator
    assert(uuid_is_valid("a38fa5da-1456-498a-9ef541f39e35bfe57") == false); // invalid seperator
    assert(uuid_is_valid("a38f-5da-1456-498a-9ef5-1f39e35bfe57") == false); // invalid seperator
    assert(uuid_is_valid("a38fa5da-14-6-498a-9ef5-1f39e35bfe57") == false); // invalid seperator
    assert(uuid_is_valid("a38fa5da-1456-49-a-9ef5-1f39e35bfe57") == false); // invalid seperator
    assert(uuid_is_valid("a38fa5da-1456-498a-9e-5-1f39e35bfe57") == false); // invalid seperator
    assert(uuid_is_valid("a38fa5da-1456-498a-9ef5-1f39e35-fe57") == false); // invalid seperator
    assert(uuid_is_valid("a38fa5da-1456-498a-9ef5-1f39e35b") == false);     // too short
    assert(uuid_is_valid("a38fa5da-1456-498a-9ef5-fe571f39e35bfe57") == false); // too long
    assert(uuid_is_valid("a38fX5da-1456-498a-9ef5-1f39e35bfe57") == false); // invalid character
    assert(uuid_is_valid("a38fa5da-14x6-498a-9ef5-1f39e35bfe57") == false); // invalid character
    assert(uuid_is_valid("a38fa5da-1456-49za-9ef5-1f39e35bfe57") == false); // invalid character
    assert(uuid_is_valid("a38fa5da-1456-498a-9ey5-1f39e35bfe57") == false); // invalid character
    assert(uuid_is_valid("a38fa5da-1456-498a-9ef5-1fq9e35bfe57") == false); // invalid character
    assert(uuid_is_valid("a38fa5da-1456-498a-9ef5-1f39e3Sbfe57") == false); // invalid character
    assert(uuid_is_valid("a38fa5da-l33t-498a-9ef5-1f39e35bfe57") == false); // invalid character

    for (auto i = 0; i < 100000; ++i) {
        char buffer[37];
        uuid_random(buffer);
        assert(uuid_is_valid(buffer));
    }
}

#if WINDOWS
int wmain ( int argc, wchar_t *argv[], wchar_t *envp[])
#else
int main ( int argc, char *argv[], char *envp[])
#endif
{
    try {
        uuid_test();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
    return 0;
}
