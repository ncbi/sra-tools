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
#include <writer.hpp>

std::string findReadGroup(unsigned const N, std::string const fields[])
{
    for (auto i = 11; i < N; ++i) {
        if (fields[i].substr(0, 5) == "RG:Z:")
            return fields[i].substr(5);
    }
    return std::string();
}

void writeRow(VDB::Writer const &out, unsigned const N, std::string const fields[])
{
    auto const &QNAME = fields[0];
    auto const flags = std::stoi(fields[1]);
    int32_t const readNo = (flags >> 6) & 3;
    auto const &RNAME = fields[2];
    int32_t const position = std::stoi(fields[3]);
    auto const &CIGAR = fields[5];
    auto const &sequence = fields[9];
    auto const isAligned = (flags & 0x04) == 0 ? ((position > 0) && RNAME != "*" && CIGAR != "*") : false;
    auto const spotGroup = findReadGroup(N, fields);
    
    if (spotGroup.size() > 0)
        out.value(1, spotGroup);
    
    out.value(2, QNAME);
    out.value(3, readNo);
    out.value(4, sequence);

    if (isAligned) {
        out.value(5, RNAME);
        out.value(6, char(((flags & 0x10) == 0) ? '+' : '-'));
        out.value(7, position - 1);
        out.value(8, CIGAR);
    }
    out.closeRow(1);
}

int process(VDB::Writer const &out, FILE *in)
{
    std::string fields[256];
    unsigned fld = 0;

    for ( ;; ) {
        auto const ch = fgetc(in);
        if (ch < 0)
            break;
        if (ch == '\n') {
            if (fld >= 10 && fields[0][0] != '@')
                writeRow(out, fld + 1, fields);
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
    return 0;
}

int process(FILE *out, FILE *in)
{
    auto const writer = VDB::Writer(out);
    
    writer.destination("IR.vdb");
    writer.schema("../shared/schema/aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("sam2ir", "1.0.0");
    
    writer.openTable(1, "RAW");
    writer.openColumn(1, 1, 8, "READ_GROUP");
    writer.openColumn(2, 1, 8, "FRAGMENT");
    writer.openColumn(3, 1, 32, "READNO");
    writer.openColumn(4, 1, 8, "SEQUENCE");
    writer.openColumn(5, 1, 8, "REFERENCE");
    writer.openColumn(6, 1, 8, "STRAND");
    writer.openColumn(7, 1, 32, "POSITION");
    writer.openColumn(8, 1, 8, "CIGAR");
    
    writer.beginWriting();
    
    writer.defaultValue<char>(1, 0, 0);
    writer.defaultValue<char>(5, 0, 0);
    writer.defaultValue<char>(6, 0, 0);
    writer.defaultValue<int32_t>(7, 0, 0);
    writer.defaultValue<char>(8, 0, 0);
    
    auto const result = process(writer, in);
    
    writer.endWriting();
    
    return result;
}

int main(int argc, char *argv[])
{
    return process(stdout, stdin);
}
