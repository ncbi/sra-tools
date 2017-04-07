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

void writeRow(VDB::Cursor const &curs, unsigned const N, std::string const fields[])
{
    auto const &QNAME = fields[0];
    auto const flags = std::stoi(fields[1]);
    int const readNo = (flags >> 6) & 3;
    auto const &RNAME = fields[2];
    auto const position = std::stoi(fields[3]);
    auto const &CIGAR = fields[5];
    auto const &sequence = fields[9];
    auto const isAligned = (flags & 0x04) == 0 ? ((position > 0) && RNAME != "*" && CIGAR != "*") : false;
    auto spotGroup = std::string();

    for (auto i = 11; i < N; ++i) {
        if (fields[i].substr(0, 5) == "RG:Z:") {
            spotGroup = fields[i].substr(5);
            break;
        }
    }
    
    curs.newRow();
    if (spotGroup.size() > 0)
        curs.write(1, spotGroup);
    else
        curs.writeNull<char>(1);
    curs.write(2, QNAME);
    curs.write(3, readNo);
    curs.write(4, sequence);
    if (isAligned) {
        curs.write(5, RNAME);
        curs.write(6, 1, ((flags & 0x10) == 0) ? "F" : "R");
        curs.write(7, position - 1);
        curs.write(8, CIGAR);
    }
    else {
        curs.writeNull<char>(5);
        curs.writeNull<char>(6);
        curs.writeNull<int>(7);
        curs.writeNull<char>(8);
    }
    curs.commitRow();
}

int process(VDB::Table const &out, std::istream &in)
{
    char const *FLDS[] = { "READ_GROUP", "FRAGMENT", "READNO", "SEQUENCE", "REFERENCE", "STRAND", "POSITION", "CIGAR" };
    auto curs = out.append(sizeof(FLDS)/sizeof(FLDS[0]), FLDS);
    std::string fields[256];
    unsigned fld = 0;

    for ( ;; ) {
        auto const ch = in.get();
        if (ch < 0)
            break;
        if (ch == '\n') {
            if (fld >= 10 && fields[0][0] != '@')
                writeRow(curs, fld + 1, fields);
            for (auto &&s : fields)
                s.clear();
            fld = 0;
            continue;
        }
        if (ch == '\t') {
            ++fld;
            if (fld == 256)
                throw std::runtime_error("too many fields");
            continue;
        }
        fields[fld] += char(ch);
    }
    curs.commit();
    return 0;
}

int main(int argc, char *argv[])
{
    auto mgr = VDB::Manager();
    auto schema = mgr.schemaFromFile("../shared/schema/aligned-ir.schema.text", "../include/");
    auto outDb = mgr.create("IR.vdb", schema, "NCBI:db:IR:raw");
    auto outTbl = outDb.create("RAW", "RAW");
    // auto in = std::ifstream("u.txt");
    
    return process(outTbl, std::cin);
}
