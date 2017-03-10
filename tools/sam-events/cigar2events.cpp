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
#include <map>
#include <string>
#include <cassert>
#include <utility>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

#include "fasta-file.hpp"
#include "cigar2events.hpp"

using namespace CPP;

#define UNREACHABLE do { (void)0; } while(0)

static int verbose = 0;

enum OpCode {
    M, I, D, N, S, H, P, match, mismatch
};

struct Operation {
    unsigned length;
    int opcode;
    
    Operation(unsigned len = 0, int code = 0) : length(len), opcode(code) {}
    operator bool() const { return length != 0; }
};

struct CIGAR_String {
    std::string const &string;
    
    CIGAR_String(std::string const &s) : string(s) {}
    
    class iterator {
        friend struct CIGAR_String;
        
        std::istringstream *iss;
        Operation currentValue;
        
        iterator() : iss(0) {}
        explicit iterator(std::string const &s) {
            iss = new std::istringstream(s);
            operator ++();
        }
    public:
        ~iterator() {
            delete iss;
        }
        Operation const &operator *() const {
            return currentValue;
        }
        bool operator !=(iterator const &) const {
            return iss && iss->good() && !iss->eof();
        }
        iterator &operator ++() {
            unsigned length;
            if (*iss >> length) {
                char code;
                if (*iss >> code) {
                    int opcode;
                    switch (code) {
                        case 'M': opcode = M; break;
                        case 'I': opcode = I; break;
                        case 'D': opcode = D; break;
                        case 'N': opcode = N; break;
                        case 'S': opcode = S; break;
                        case 'H': opcode = H; break;
                        case 'P': opcode = P; break;
                        case '=': opcode = match; break;
                        case 'X': opcode = mismatch; break;
                        default:
                            throw std::domain_error("invalid op code");
                    }
                    currentValue = Operation(length, opcode);
                }
                else
                    throw std::domain_error("expected an op code");
            }
            else if (!iss->eof())
                throw std::domain_error("expected a number");
            return *this;
        }
    };
    iterator begin() const {
        return iterator(string);
    }
    iterator end() const {
        return iterator();
    }
};

std::pair<unsigned, unsigned> CPP::measureCIGAR(std::string const &cigar)
{
    unsigned rpos = 0;
    unsigned spos = 0;
    unsigned count = 0;
    
    CIGAR_String const &C = CIGAR_String(cigar);
    CIGAR_String::iterator const &e = C.end();
    
    for (CIGAR_String::iterator i = C.begin(); i != e; ++i) {
        Operation const &op = *i;
        
        if (op.length == 0)
            throw std::domain_error("invalid length");
        
        switch (op.opcode) {
            case M:
            case match:
            case mismatch:
                spos += op.length;
                rpos += op.length;
                break;
            case D:
            case N:
                rpos += op.length;
                break;
            case S:
            case I:
                spos += op.length;
                break;
            case H:
            default:
                break;
        }
        ++count;
    }

    return std::make_pair(rpos, spos);
}


std::vector<Event> CPP::expandAlignment(  FastaFile::Sequence const &reference
                                        , unsigned const position
                                        , std::string const &cigar
                                        , char const *const querySequence)
{
    std::vector<Event> result;
    unsigned rpos = 0;
    unsigned spos = 0;
    
    CIGAR_String const &C = CIGAR_String(cigar);
    CIGAR_String::iterator const &e = C.end();
    
    for (CIGAR_String::iterator i = C.begin(); i != e; ++i) {
        Operation const &op = *i;
        unsigned end_rpos = rpos;
        unsigned end_spos = spos;

        switch (op.opcode) {
            case M:
            case match:
            case mismatch:
                end_spos += op.length;
                end_rpos += op.length;
                break;
            case I:
                end_spos += op.length;
                end_rpos += 1;
                break;
            case D:
                end_spos += 1;
                end_rpos += op.length;
                break;
            case N:
                rpos += op.length;
                continue; // N's don't generate an event
            case S:
                spos += op.length;
                continue; // S's don't generate an event
            default:
                continue; // all other operations are no-ops
        }
        while (rpos < end_rpos && spos < end_spos) {
            Event e;
            
            e.type = Event::none;
            e.length = op.length;
            e.refPos = rpos;
            e.seqPos = spos;
            switch (op.opcode) {
                case M:
                case match:
                case mismatch:
                    e.type = reference.data[position + rpos] == querySequence[spos] ? Event::match : Event::mismatch;
                    e.length = 1;
                    ++spos;
                    ++rpos;
                    break;
                case I:
                    e.type = Event::insertion;
                    spos += op.length;
                    break;
                case D:
                    e.type = Event::deletion;
                    rpos += op.length;
                    break;
                default:
                    UNREACHABLE;
            }
            if (result.size() > 0) {
                Event &last = result.back();
                if (last.type == e.type && last.refEnd() == e.refPos && last.seqEnd() == e.seqPos) {
                    last.length += e.length;
                    continue;
                }
            }
            result.push_back(e);
        }
    }
    return result;
}
