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
#include <stdexcept>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <vdb.hpp>

std::string itos(int64_t i)
{
    char buffer[256];
    int n = snprintf(buffer, 256, "%lli", i);
    return std::string(buffer);
}

VDB::Cursor openOutputCursor(VDB::Table const &out)
{
    static char const *const FLDS[] = { "READ_GROUP", "FRAGMENT", "READNO", "SEQUENCE", "REFERENCE", "STRAND", "POSITION", "CIGAR" };
    return out.append(sizeof(FLDS)/sizeof(FLDS[0]), FLDS);
}

VDB::Cursor openInputCursorAligned(VDB::Database const &in, bool primary)
{
    static char const *const FLDS[] = { "SEQ_SPOT_GROUP", "SEQ_SPOT_ID", "SEQ_READ_ID", "READ", "REF_NAME", "REF_ORIENTATION", "REF_POS", "CIGAR_SHORT" };
    auto tbl = in[primary ? "PRIMARY_ALIGNMENT" : "SECONDARY_ALIGNMENT"];
    return tbl.read(sizeof(FLDS)/sizeof(FLDS[0]), FLDS);
}

VDB::Cursor openInputCursorSequence(VDB::Database const &in)
{
    static char const *const FLDS[] = { "SPOT_GROUP", "READ", "(U32)READ_START", "(U32)READ_LENGTH", "PRIMARY_ALIGNMENT_ID" };
    auto tbl = in["SEQUENCE"];
    return tbl.read(sizeof(FLDS)/sizeof(FLDS[0]), FLDS);
}

int process(VDB::Table const &out, VDB::Database const &inDb)
{
    auto curs = openOutputCursor(out);
    
    {
        auto in = openInputCursorSequence(inDb);
        int64_t endRow = 0;

        for (int64_t row = in.rowRange(&endRow); row < endRow; ++row) {
            VDB::Cursor::RawData data[5];
            
            in.read(row, 6, data);
            
            char const *rawSpotGroup = (char const *)data[0].data;
            char const *rawSequence = (char const *)data[1].data;
            uint32_t const *readStart = (uint32_t const *)data[2].data;
            uint32_t const *readLen = (uint32_t const *)data[3].data;
            int64_t const *pid = (int64_t const *)data[4].data;
            
            unsigned const nreads = data[2].elements;
            
            auto const &spotGroup = std::string((char const *)data[0].data, data[0].elements);
            
            for (unsigned i = 0; i < nreads; ++i) {
                if (pid[i] == 0) {
                    auto const &sequence = std::string(rawSequence + readStart[i], readLen[i]);
                    
                    curs.nextRow();
                    curs.write(1, spotGroup);
                    curs.write(2, itos(row));
                    curs.write(3, i + 1);
                    curs.write(4, sequence);
                    curs.writeNull<char>(5);
                    curs.writeNull<char>(6);
                    curs.writeNull<int>(7);
                    curs.writeNull<char>(8);
                    curs.commitRow();
                }
            }
        }
    }
    try {
        auto in = openInputCursorAligned(inDb, false);
    }
    catch (...) {}
    try {
        auto in = openInputCursorAligned(inDb, true);
    }
    catch (...) {}
    curs.commit();
    return 0;
}

int main(int argc, char *argv[])
{
    auto mgr = VDB::Manager();
    auto schema = mgr.schemaFromFile("../shared/schema/aligned-ir.schema.text", "../include/");
    auto outDb = mgr.create("IR.vdb", schema, "NCBI:db:IR:raw");
    auto outTbl = outDb.create("RAW", "RAW");
    auto inDb = mgr[argv[1]];
    
    return process(outTbl, std::cin);
}
