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
 *  Loader QA Stats
 *
 * Purpose:
 *  Parse inputs.
 */

#include <iosfwd>
#include <string>
#include <vector>

struct CIGAR {
    struct OP {
        unsigned length: 28, opcode: 4;

        unsigned sequenceLength() const {
            switch (opcode) {
            case 0:
            case 1:
            case 4:
            case 7:
            case 8:
                return length;
            default:
                return 0;
            }
        }
        unsigned referenceLength() const {
            switch (opcode) {
            case 0:
            case 2:
            case 3:
            case 7:
            case 8:
                return length;
            default:
                return 0;
            }
        }

        friend std::istream &operator >>(std::istream &is, OP &out);
    };
    std::vector<OP> operations;

    unsigned sequenceLength() const {
        unsigned result = 0;

        for (auto op : operations)
            result += op.sequenceLength();

        return result;
    }
    unsigned referenceLength() const {
        unsigned result = 0;

        for (auto op : operations)
            result += op.referenceLength();

        return result;
    }

    friend std::istream &operator >>(std::istream &is, CIGAR &out);
};

struct Input {
    enum struct ReadType {
        biological,
        technical,
        aligned
    };
    enum struct ReadOrientation {
        forward,
        reverse
    };
    struct Read {
        int start, length, position = -1, reference = -1;
        ReadType type;
        ReadOrientation orientation;
        CIGAR cigar;
    };
    std::string sequence;
    std::vector<Read> reads;
    int group;

    static std::vector<std::string> references;
    static std::vector<std::string> groups;

    static Input readFrom(std::istream &, bool isfirst = false);
    static void runTests();
};
