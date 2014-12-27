/*==============================================================================
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

#include "fasta-file.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

/*
 * Fasta files:
 *  Fasta file consists of one of more sequences.  A sequence in a fasta file
 *  consists of a seqid line followed by lines containing the bases of the
 *  sequence.  A seqid line starts with '>' and the next word (whitespace
 *  delimited) is the seqid.
 */

struct tmpSequence {
    std::string SEQID;
    std::string SEQID_LINE;
    size_t data_start;
    unsigned data_size;
};

static std::string const whitespace(" \t");

static std::string const getline(std::istream &is)
{
    std::string line("");

    std::getline(is, line);

    auto const lineend = line.find_last_not_of(whitespace);
    if (lineend != std::string::npos)
        line.erase(lineend + 1);

    return line;
}

static bool readFile(std::istream &is,
                     char *&data,
                     size_t &size,
                     size_t &limit,
                     std::vector<tmpSequence> &sequences
                     )
{
    tmpSequence sequence;
    auto st = 0;

    for ( ; ; ) {
        auto line = getline(is);

        if (line.size() == 0 && is.good())
            continue;

        switch (st) {
            case 1:
                if (line[0] == '>') {
                    auto const seqidstart = line.find_first_not_of(whitespace, 1);

                    if (seqidstart != std::string::npos) {
                        sequence.SEQID_LINE.push_back(' ');
                        sequence.SEQID_LINE.append(line, seqidstart, std::string::npos);
                    }
                    break;
                }
                ++st;
                /* fallthrough */
            case 2:
                if (line[0] == '>' || line.size() == 0) {
                    sequences.push_back(sequence);
                    st = 0;
                    /* fallthrough */
                }
                else {
                    auto const start = line.find_first_not_of(whitespace);
                    auto const len = line.size() - start;

                    if (size + len > limit) {
                        do {
                            limit <<= 1;
                        } while (size + len > limit);
                        auto const tmp = realloc(reinterpret_cast<void *>(data), limit);

                        if (!tmp) throw std::bad_alloc();
                        data = reinterpret_cast<char *>(tmp);
                    }
                    memcpy(data + size, line.data() + start, len);
                    size += len;
                    sequence.data_size += len;
                    break;
                }
            case 0:
                if (line.size() == 0)
                    return true;

                if (line[0] == '>') {
                    auto const seqidstart = line.find_first_not_of(whitespace, 1);
                    if (seqidstart != std::string::npos) {
                        auto const seqidend = line.find_first_of(whitespace, seqidstart);

                        sequence.SEQID_LINE = std::string(line, seqidstart);
                        sequence.SEQID = std::string(line, seqidstart, seqidend - seqidstart);
                        sequence.data_start = size;
                        sequence.data_size = 0;
                        ++st;
                        break;
                    }
                }
            default:
                return false;
        }
    }
}

FastaFile::FastaFile(std::istream &is)
{
    std::vector<tmpSequence> tmp;
    size_t limit = 1024u * 1024u;
    size_t size = 0;
    auto data = reinterpret_cast<char *>(malloc(limit));

    if (!data) throw std::bad_alloc();
    if (readFile(is, data, size, limit, tmp) && is.eof()) {
        this->data = realloc(reinterpret_cast<void *>(data), size);
        if (!this->data) throw std::bad_alloc();

        for (auto i = tmp.begin(); i != tmp.end(); ++i) {
            Sequence seq;

            seq.SEQID = i->SEQID;
            seq.SEQID_LINE = i->SEQID_LINE;
            seq.data = data + i->data_start;
            seq.datasize = i->data_size;

            sequences.push_back(seq);
        }
    }
    else {
        this->data = nullptr;
        free(data);
    }
}

FastaFile const FastaFile::load(std::string const filename)
{
    std::ifstream ifs(filename);

    return ifs.is_open() ? FastaFile::load(ifs) : FastaFile();
}

#ifdef TESTING
int main(int argc, char *argv[])
{
    if (argc > 1) {
        auto test = FastaFile::load(argv[1]);

        std::cout << "Loaded " << test.sequences.size() << " sequences" << std::endl;

        size_t total = 0;
        for (auto i = test.sequences.begin(); i != test.sequences.end(); ++i)
            total += i->datasize;

        std::cout << "Loaded " << total << " bases" << std::endl;
    }
}
#endif
