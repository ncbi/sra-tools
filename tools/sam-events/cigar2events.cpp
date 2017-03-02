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
#include <cstdint>
#include <cassert>
#include <utility>
#include <sstream>
#include <stdexcept>
#include <cstring>

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
        friend CIGAR_String;
        
        std::istringstream iss;
        Operation currentValue;
        
        iterator() : iss(std::string()) {}
        iterator(std::string const &s) : iss(s) {
            operator ++();
        }
    public:
        Operation const &operator *() const {
            return currentValue;
        }
        bool operator !=(iterator const &) const {
            return iss.good() && !iss.eof();
        }
        iterator &operator ++() {
            unsigned length;
            if (iss >> length) {
                char code;
                if (iss >> code) {
                    int opcode;
                    switch (code) {
                        case 'M': opcode = OpCode::M; break;
                        case 'I': opcode = OpCode::I; break;
                        case 'D': opcode = OpCode::D; break;
                        case 'N': opcode = OpCode::N; break;
                        case 'S': opcode = OpCode::S; break;
                        case 'H': opcode = OpCode::H; break;
                        case 'P': opcode = OpCode::P; break;
                        case '=': opcode = OpCode::match; break;
                        case 'X': opcode = OpCode::mismatch; break;
                        default:
                            throw std::domain_error("invalid op code");
                    }
                    currentValue = Operation(length, opcode);
                }
                else
                    throw std::domain_error("expected an op code");
            }
            else if (!iss.eof())
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
    int lclip = -1;
    int rclip = -1;
    
    for (auto &&op : CIGAR_String(cigar)) {
        if (op.length == 0)
            throw std::domain_error("invalid length");
        
        switch (op.opcode) {
            case OpCode::M:
            case OpCode::match:
            case OpCode::mismatch:
                spos += op.length;
                rpos += op.length;
                break;
            case OpCode::D:
            case OpCode::N:
                rpos += op.length;
                break;
            case OpCode::S:
                if (lclip < 0)
                    lclip = count;
                else
                    rclip = count;
            case OpCode::I:
                spos += op.length;
                break;
            case OpCode::H:
                if (lclip < 0)
                    lclip = count;
                else
                    rclip = count;
                break;
            default:
                break;
        }
        ++count;
    }
    if (lclip > 0 || !(rclip == -1 || rclip == count - 1))
        throw std::domain_error("invalid clipping");

    return std::make_pair(rpos, spos);
}


std::vector<Event> CPP::expandAlignment(  FastaFile::Sequence const &reference
                                        , unsigned const position
                                        , std::string const &cigar
                                        , char const *const querySequence)
{
    auto result = std::vector<Event>();
    unsigned rpos = 0;
    unsigned spos = 0;
    
    for (auto &&op : CIGAR_String(cigar)) {
        unsigned end_rpos = rpos;
        unsigned end_spos = spos;

        switch (op.opcode) {
            case OpCode::M:
            case OpCode::match:
            case OpCode::mismatch:
                end_spos += op.length;
                end_rpos += op.length;
                break;
            case OpCode::I:
                end_spos += op.length;
                end_rpos += 1;
                break;
            case OpCode::D:
                end_spos += 1;
                end_rpos += op.length;
                break;
            case OpCode::N:
                rpos += op.length;
                continue; // N's don't generate an event
            case OpCode::S:
                spos += op.length;
                continue; // S's don't generate an event
            default:
                continue; // all other operations are no-ops
        }
        while (rpos < end_rpos && spos < end_spos) {
            Event e;
            
            e.type = Event::Type::none;
            e.length = op.length;
            e.refPos = rpos;
            e.seqPos = spos;
            switch (op.opcode) {
                case OpCode::M:
                case OpCode::match:
                case OpCode::mismatch:
                    e.type = reference.data[position + rpos] == querySequence[spos] ? Event::Type::match : Event::Type::mismatch;
                    e.length = 1;
                    ++spos;
                    ++rpos;
                    break;
                case OpCode::I:
                    e.type = Event::Type::insertion;
                    spos += op.length;
                    break;
                case OpCode::D:
                    e.type = Event::Type::deletion;
                    rpos += op.length;
                    break;
                default:
                    UNREACHABLE;
            }
            if (result.size() > 0) {
                auto &last = result.back();
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
