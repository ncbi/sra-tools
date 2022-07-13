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

#pragma once
#include <string>
#include <iosfwd>
#include <cstdint>

namespace sratools {

struct Version {
    uint32_t packed;
    operator std::string() const noexcept;
    
    friend std::ostream &operator <<(std::ostream &os, Version const &vers) {
        return os << vers.major() << '.' << vers.minor() << '.' << vers.revision();
    }
    
    bool operator <(Version const &rhs) const { return packed < rhs.packed; }
    bool operator >(Version const &rhs) const { return packed > rhs.packed; }
    bool operator <=(Version const &rhs) const { return packed <= rhs.packed; }
    bool operator >=(Version const &rhs) const { return packed >= rhs.packed; }
    bool operator ==(Version const &other) const { return packed == other.packed; }
    bool operator !=(Version const &other) const { return packed != other.packed; }

    static Version const &current;
    
    explicit Version(uint32_t const pckd)
    : packed(pckd)
    {}
    
    Version(uint8_t const major, uint8_t const minor, uint16_t const revision)
    : packed((((uint32_t)major) << 24) | (((uint32_t)minor) << 16) | ((uint32_t)revision))
    {}
    
    unsigned major() const { return packed >> 24; }
    unsigned minor() const { return (packed >> 16) & 0xFF; }
    unsigned revision() const { return packed & 0xFFFF; }
    
    Version nextMinor() const { return Version(major(), minor() + 1, 0); }
    Version nextMajor() const { return Version(major() + 1, 0, 0); }
};

} // namespace sratools
