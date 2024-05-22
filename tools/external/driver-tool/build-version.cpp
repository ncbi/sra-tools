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
 *  locate executable
 *
 */

#include <string>
#include <vector>
#include <algorithm>
#include <utility>
#include <iterator>
#include <iostream>
#include <cstdio>
#include "build-version.hpp"
#include "../../shared/toolkit.vers.h"

struct VersionBuffer {
private:
    // 255.255.65535\0
    char buffer[3 + 1 + 3 + 1 + 5 + 1];
    char *begin;

    char *end() noexcept { return &buffer[sizeof(buffer)]; }

    static inline char *utoa(unsigned x, char const suffix, char *dst) noexcept {
        *--dst = suffix;
        do { *--dst = (x % 10) + '0'; } while ((x /= 10) != 0);
        return dst;
    }

public:
    char const *formatted() const noexcept {
        return begin;
    }

    VersionBuffer(sratools::Version const &vers) noexcept {
        begin = utoa(vers.ncbi_major(), '.', utoa(vers.ncbi_minor(), '.', utoa(vers.revision(), '\0', end())));
    }
};

template< typename T >
struct Range
{
    T begin;
    T end;

    T size() const noexcept { return end - begin; }
};

template< typename T >
static T parse_int(T start, T const end, int *const p) noexcept
{
    if (isdigit(*start)) {
        int result = 0;
        while (start != end && *start != '\0' && isdigit(*start)) {
            result = (result * 10) + (*start++ - '0');
            *p = result;
        }
    }
    return start;
}

namespace sratools {

Version::operator std::string() const noexcept {
    VersionBuffer buffer(*this);
    return std::string(buffer.formatted());
}

std::ostream &Version::write(std::ostream &os) const noexcept {
    VersionBuffer buffer(*this);
    return os << buffer.formatted();
}

Version const &Version::current = Version(TOOLKIT_VERS);
std::string const &Version::currentString = std::string(Version::current);

Version Version::removeVersion(std::string &name) noexcept {
    Version result;
    unsigned mparts = 0;
    auto region = Range<std::string::const_iterator>();

    for (auto i = name.cbegin(); i != name.cend(); ++i) {
        if (*i != '.') continue;

        unsigned parts = 0;
        auto const parseResult = make(i + 1, name.cend(), &parts);
        if (parts > 0 && parts >= mparts) {
            mparts = parts;
            region = {i + 1, parseResult.second};
            result = parseResult.first;
        }
    }
    if (mparts > 0) {
        // remove version from name, the '.' before the version is also removed
        auto const p1 = std::string(name.cbegin(), region.begin - 1);
        auto const p2 = std::string(region.end, name.cend());
        name = p1 + p2;
    }
    return result;
}

template< typename T >
std::pair< Version, T > Version::make(T begin, T endp, unsigned *parts) noexcept
{
    int maj = -1, mnr = -1, rev = -1;

    *parts = 0;
    auto const s1 = parse_int(begin, endp, &maj);
    if (0 <= maj && maj < 0x100) {
        *parts = 1;
        if (s1 < endp && *s1 == '.') {
            auto const s2 = parse_int(s1 + 1, endp, &mnr);
            if (0 <= mnr && mnr < 0x100) {
                *parts = 2;
                if (s2 < endp && *s2 == '.') {
                    auto const s3 = parse_int(s2 + 1, endp, &rev);
                    if (0 <= rev && rev < 0x10000) {
                        *parts = 3;
                        return { Version((uint8_t)maj, (uint8_t)mnr, (uint16_t)rev), s3 };
                    }
                }
                return { Version((uint8_t)maj, (uint8_t)mnr, 0), s2 };
            }
        }
        return { Version((uint8_t)maj, 0, 0), s1 };
    }
    return { Version(), begin };
}

Version::Version(char const *string, char const *const endp)
{
    unsigned parts = 0;
    auto const parsed = make(string, endp, &parts);
    if (parts > 0 && (parsed.second == endp || ispunct(*parsed.second) || isspace(*parsed.second)))
        packed = parsed.first.packed;
    else
        throw std::domain_error("not a version string");
}

} // namespace sratools

