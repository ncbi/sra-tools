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

#include <string>
#include <vector>
#include <iostream>
#include <map>

/*
 * Fasta files:
 *  Fasta file consists of one of more sequences.  A sequence in a fasta file
 *  consists of a seqid line followed by lines containing the bases of the
 *  sequence.  A seqid line starts with '>' and the next word (whitespace
 *  delimited) is the seqid.
 */

class FastaFile {
    FastaFile() : data(NULL) {}
    explicit FastaFile(std::istream &is);
    
    void *data;

public:
    std::map<std::string, unsigned> index() const;
    
    struct Sequence {
        std::string SEQID;
        std::string defline;
        char const *data;
        unsigned length;
        bool hadErrors; // erroneous base values are replaced with N
    };

    std::vector<Sequence const> sequences;

    ~FastaFile() {
        free(data);
        data = nullptr;
    }

    static FastaFile load(std::istream &is) {
        return FastaFile(is);
    }
    static FastaFile load(std::string const &filename);
};

class IndexedFastaFile : public FastaFile {
    std::map<std::string, unsigned> index;

    explicit IndexedFastaFile(FastaFile &&ff)
    : FastaFile(ff)
    , index(ff.index())
    {}
public:
    int find(std::string const &SEQID) const {
        auto const iter = index.find(SEQID);
        return iter != index.end() ? iter->second : -1;
    }
    
    static IndexedFastaFile load(std::istream &is) {
        return IndexedFastaFile(FastaFile::load(is));
    }
    static IndexedFastaFile load(std::string const &filename) {
        return IndexedFastaFile(FastaFile::load(filename));
    }
};
