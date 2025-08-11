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

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <cstdint>

struct CIGAR {
    struct OP {
        unsigned length: 28, opcode: 4;

        int sequenceLength() const {
            switch (opcode) {
            case 0:
            case 1:
            case 4:
            case 7:
            case 8:
                return (int)length;
            default:
                return 0;
            }
        }
        int referenceLength() const {
            switch (opcode) {
            case 0:
            case 2:
            case 3:
            case 7:
            case 8:
                return (int)length;
            default:
                return 0;
            }
        }

        uint32_t hashValue() const {
            return (uint32_t(length) << 4) | opcode;
        }

        friend std::istream &operator >>(std::istream &is, OP &out);
    };
    std::vector<OP> operations;

    operator bool() const { return operations.size() > 0; }
    int sequenceLength() const {
        int result = 0;

        for (auto op : operations)
            result += op.sequenceLength();

        return result;
    }
    int referenceLength() const {
        int result = 0;

        for (auto op : operations)
            result += op.referenceLength();

        return result;
    }
    uint32_t hashValue() const {
        uint64_t value = 0xcbf29ce484222325ull;

        for (auto op : operations)
            value = (value ^ op.hashValue()) * 0x100000001b3ull;

        return uint32_t(value) | uint32_t(value >> 32);
    }

    friend std::istream &operator >>(std::istream &is, CIGAR &out);
};

struct Input {
    enum struct ReadType {
        biological,
        technical,
        aligned
    };
    friend std::ostream &operator <<(std::ostream &os, ReadType readType) {
        switch (readType) {
        case ReadType::biological:
            return os << "read type: biological";
        case ReadType::technical:
            return os << "read type: technical";
        case ReadType::aligned:
            return os << "read type: aligned";
        default:
            return os << "read type: " << (int)readType << "?";
        }
    }
    enum struct ReadOrientation {
        forward,
        reverse
    };
    friend std::ostream &operator <<(std::ostream &os, ReadOrientation orientation) {
        switch (orientation) {
        case ReadOrientation::forward:
            return os << "read orientation: forward";
        case ReadOrientation::reverse:
            return os << "read orientation: reverse";
        default:
            return os << "read orientation: " << (int)orientation << "?";
        }
    }
    struct Read {
        int start, length, position = -1, reference = -1;
        ReadType type = ReadType::biological;
        ReadOrientation orientation = ReadOrientation::forward;
        CIGAR cigar;
    };
    std::string sequence;
    std::vector<Read> reads;
    int group = -1;
    
    static std::vector<std::string> references;
    static std::vector<std::string> groups;

    static int getGroup(std::string_view const &named);
    static int getReference(std::string_view const &named);

    static void SAM_HeaderLine(std::string const &line) {}
    static void runTests();

    struct Source {
        uint64_t records = 0;

        virtual operator bool() const = 0;
        virtual bool eof() const = 0;
        virtual Input get() = 0;
        virtual ~Source() {}

        struct StdInType {};
        struct FilePathType {
            char const *path;
            bool use_mmap;
        };
        struct StringLiteralType {
            std::string const &data;
        };
        using Type = std::variant<StdInType, FilePathType, StringLiteralType>;
    protected:
        Source() {}
    };
    static std::unique_ptr<Source> newSource(Source::Type const &source = Source::StdInType{}, bool multithreaded = true);

    struct Reset {
        ~Reset() {
            Input::references.clear();
            Input::groups.clear();
        }
        friend Input;
    private:
        Reset() {}
    };
    static Reset getReset() { return Reset(); }
private:
    static std::string readLine(std::istream &);
};

