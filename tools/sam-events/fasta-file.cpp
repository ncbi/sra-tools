/* =============================================================================
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
 * =============================================================================
 */

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <cctype>
#include <cstdlib>

#include "fasta-file.hpp"

using namespace CPP;

/*
 * Fasta files:
 *  Fasta file consists of one of more sequences.  A sequence in a fasta file
 *  consists of a seqid line followed by lines containing the bases of the
 *  sequence.  A seqid line starts with '>' and the next word (whitespace
 *  delimited) is the seqid.
 */

static char const tr4na[] = {
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','.',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ','A','B','C','D',' ',' ','G','H',' ',' ','K',' ','M','N',' ',' ',' ','R','S','T','T','V','W','N','Y',' ',' ',' ',' ',' ',' ',
    ' ','A','B','C','D',' ',' ','G','H',' ',' ','K',' ','M','N',' ',' ',' ','R','S','T','T','V','W','N','Y',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
};

FastaFile::FastaFile(std::istream &is) : data(0)
{
    char *data = 0;
    size_t size = 0;
    
    {
        size_t limit = 1024u;
        void *mem = malloc(limit);
        if (mem)
            data = reinterpret_cast<char *>(mem);
        else
            throw std::bad_alloc();

        for ( ; ; ) {
            int const ch = is.get();
            if (ch == std::char_traits<char>::eof())
                break;
            if (size + 1 < limit)
                data[size++] = char(ch);
            else {
                size_t const newLimit = limit << 1;
                
                mem = realloc(mem, newLimit);
                if (mem) {
                    data = reinterpret_cast<char *>(mem);
                    limit = newLimit;
                    data[size++] = char(ch);
                }
                else
                    throw std::bad_alloc();
            }
        }
        mem = realloc(mem, size);
        if (mem) {
            this->data = mem;
            data = reinterpret_cast<char *>(mem);
        }
        else
            throw std::bad_alloc();
    }
    std::vector<size_t> defline;
    {
        int st = 0;
        for (size_t i = 0; i < size; ++i) {
            int const ch = data[i];
            if (st == 0) {
                if (ch == '\r' || ch == '\n')
                    continue;
                if (ch == '>')
                    defline.push_back(i);
                ++st;
            }
            else if (ch == '\r' || ch == '\n')
                st = 0;
        }
        defline.push_back(size);
    }
    unsigned const deflines = defline.size() - 1;
    {
        for (unsigned i = 0; i < deflines; ++i) {
            Sequence seq = Sequence();
            
            size_t const offset = defline[i];
            size_t const length = defline[i + 1] - offset;
            char *base = data + offset;
            char *const endp = base + length;
            
            while (base < endp) {
                int const ch = *base++;
                if (ch == '\r' || ch == '\n')
                    break;
                seq.defline += ch;
            }
            seq.data = base;
            char *dst = base;
            {
                int j = 1;
                while (j < seq.defline.size() && isspace(seq.defline[j]))
                    ++j;
                while (j < seq.defline.size() && !isspace(seq.defline[j]))
                    seq.SEQID += seq.defline[j++];
            }

            while (base < endp) {
                int const chi = *base++;
                if (chi == '\r' || chi == '\n')
                    continue;
                int const ch = tr4na[chi];
                if (ch != ' ')
                    *dst++ = ch;
                else {
                    seq.hadErrors = true;
                    *dst++ = 'N';
                }
            }
            seq.length = dst - seq.data;
            
            sequences.push_back(seq);
        }
    }
}

std::map<std::string, unsigned> FastaFile::makeIndex() const {
    std::map<std::string, unsigned> rslt;
    for (unsigned i = 0; i < sequences.size(); ++i) {
        rslt.insert(std::make_pair(sequences[i].SEQID, i));
    }
    return rslt;
}
