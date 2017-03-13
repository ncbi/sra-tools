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
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <vdb.hpp>

std::string id2name(uint64_t id) {
    uint32_t const poly = 0xD0440405;
    uint32_t lfsr = id ^ uint64_t(cos(id) * UINT64_MAX);
    static char const set[/* 64 */] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ+abcdefghijklmnopqrstuvwxyz-0123456789";
    std::string result;
    
    result.reserve(24);
    for (int i = 0; i < 24; ++i) {
        auto j = (lfsr ^ id ^ 0xcbf29ce484222325ull) * 0x100000001b3ull;
        result.push_back(set[(j ^ j >> 32) % 64]);
        lfsr = (lfsr & 1) ? ((lfsr >> 1) ^ poly) : (lfsr >> 1);
    }
    return result;
}

int processSRA(VDB::Database const &csra, VDB::Database const &out)
{
    auto pa = csra["PRIMARY_ALIGNMENT"];
    return 0;
}

int main(int argc, char *argv[])
{
    auto mgr = VDB::Manager();
    auto outDb = mgr.create("foo.vdb", mgr.schemaFromFile("foo.vschema"), "ncbi:test:db:foo");

    return processSRA(mgr[argv[1]], outDb);
}
