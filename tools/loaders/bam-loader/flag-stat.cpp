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
 *  Loader Quality Checks.
 *
 * Purpose:
 *  Generate SAM FLAG bits counts.
 */

#include "flag-stat.h"
#include "flag-stat.hpp"
#include <cassert>

char const *FlagStat_flagBitSymbolicName(int const bitNumber) {
    static char const *const values[] = {
        "MULTIPLE_READS",
        "PROPERLY_ALIGNED",
        "UNMAPPED",
        "NEXT_UNMAPPED",
        "REVERSE_COMPLEMENTED",
        "NEXT_REVERSE_COMPLEMENTED",
        "FIRST_SEGMENT",
        "LAST_SEGMENT",
        "SECONDARY_ALIGNMENT",
        "QC_FAILED",
        "DUPLICATE",
        "SUPPLEMENTARY_ALIGNMENT",
        "BIT_12_UNUSED",
        "BIT_13_UNUSED",
        "BIT_14_UNUSED",
        "BIT_15_UNUSED",
    };
    auto constexpr const N = unsigned(sizeof(values)/sizeof(values[0]));
    assert(0 <= bitNumber && bitNumber < N);
    return (bitNumber < N) ? values[bitNumber] : "INVALID_FLAG_BIT";
}
char const *FlagStat_flagBitDescription(int const bitNumber) {
    static char const *const values[] = {
        "template having multiple segments in sequencing",
        "each segment properly aligned according to the aligner",
        "segment unmapped",
        "next segment in the template unmapped",
        "SEQ being reverse complemented",
        "SEQ of the next segment in the template being reverse complemented",
        "the first segment in the template",
        "the last segment in the template",
        "secondary alignment",
        "not passing filters, such as platform/vendor quality controls",
        "PCR or optical duplicate",
        "supplementary alignment"
    };
    auto constexpr const N = unsigned(sizeof(values)/sizeof(values[0]));
    assert(0 <= bitNumber);
    return (bitNumber < N) ? values[bitNumber] : FlagStat_flagBitSymbolicName(bitNumber);
}

FlagStat *FlagStat_Make() {
    try { return new FlagStat{}; }
    catch (...) {}
    return nullptr;
}

bool FlagStat_Add(FlagStat *self, int flag) {
    assert(flag >= 0 && flag <= 0xFFFF);
    return self->add(flag);
}

void FlagStat_rawCounts(FlagStat const *const self, uint64_t counts[16]) {
    self->rawCounts(counts);
}

void FlagStat_canonicalCounts(FlagStat const *const self, uint64_t counts[16]) {
    self->canonicalCounts(counts);
}
