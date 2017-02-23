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

#include <vector>
#include <string>
#include <cstdint>
#include <cassert>

#include <sstream>

#include "fasta-file.hpp"

struct CIGAR{
    enum OpCode {
        M = 0,
        I = 1,
        D = 2,
        N = 3,
        S = 4,
        H = 5,
        P = 6,
        match = 7,
        mismatch = 8,
        lastDefined = mismatch,
        invalid
    };
    struct packed {
        uint32_t value;
        
        enum OpCode opCode() const {
            auto const result = value & 0x0F;
            return (OpCode)result;
        }
        unsigned length() const {
            return value >> 4;
        }
        packed(unsigned length, int code) : value(0) {
            if (length << 4 == 0)
                return;
            
            switch (code) {
                case 'M': value = (length << 4) | OpCode::M; break;
                case 'I': value = (length << 4) | OpCode::I; break;
                case 'D': value = (length << 4) | OpCode::D; break;
                case 'N': value = (length << 4) | OpCode::N; break;
                case 'S': value = (length << 4) | OpCode::S; break;
                case 'H': value = (length << 4) | OpCode::H; break;
                case 'P': value = (length << 4) | OpCode::P; break;
                case '=': value = (length << 4) | OpCode::match; break;
                case 'X': value = (length << 4) | OpCode::mismatch; break;
            }
        }
    };
    std::vector<packed> value;
    
    CIGAR(std::string const &cigarString) {
        auto value = std::vector<packed>();
        auto strm = std::istringstream(cigarString);
        while (strm.good()) {
            unsigned length = 0;
            
            if (strm >> length) {
                char code = 0;

                if(strm >> code) {
                    auto const op = packed(length, code);
                    if (op.value == 0)
                        return; // parsable but invalid
                    value.push_back(op);
                }
                else
                    return;
            }
            else if (strm.eof()) {
                this->value = value;
                return; // returns here if no error
            }
            else
                return; // not parsable
        }
    }
};

#if TESTING_CIGAR
// test single operation; expects success
static void test1(void) {
    auto const cigar = CIGAR("35M");
    assert(cigar.value.size() == 1);
    assert(cigar.value[0].length() == 35);
    assert(cigar.value[0].opCode() == CIGAR::OpCode::M);
}

// test multiple operations; expects success
static void test2(void) {
    auto const cigar = CIGAR("5S35M20I5M");
    assert(cigar.value.size() == 4);

    assert(cigar.value[0].length() == 5);
    assert(cigar.value[0].opCode() == CIGAR::OpCode::S);

    assert(cigar.value[1].length() == 35);
    assert(cigar.value[1].opCode() == CIGAR::OpCode::M);
    
    assert(cigar.value[2].length() == 20);
    assert(cigar.value[2].opCode() == CIGAR::OpCode::I);
    
    assert(cigar.value[3].length() == 5);
    assert(cigar.value[3].opCode() == CIGAR::OpCode::M);
}

// test invalid opcode; expects failure
static void test3(void) {
    auto const cigar = CIGAR("35Z");
    assert(cigar.value.size() == 0);
}

// test invalid length; expects failure
static void test4(void) {
    {
        auto const cigar = CIGAR("35M0M");
        assert(cigar.value.size() == 0);
    }
    {
        auto const cigar = CIGAR("35M+M");
        assert(cigar.value.size() == 0);
    }
    {
        auto const cigar = CIGAR("35M-M");
        assert(cigar.value.size() == 0);
    }
    {
        auto const cigar = CIGAR("35MOM");
        assert(cigar.value.size() == 0);
    }
}

// test garbage; expects failure
static void test5(void) {
    auto const cigar = CIGAR("foobar");
    assert(cigar.value.size() == 0);
}

// test '*'; expects failure; strangely, it's not really an error
static void test6(void) {
    auto const cigar = CIGAR("*");
    assert(cigar.value.size() == 0);
}

int main(int argc, char *argv[]) {
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
}
#endif
