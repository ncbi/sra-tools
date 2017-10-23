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

#ifndef __VDB_HPP_INCLUDED__
#define __VDB_HPP_INCLUDED__ 1

#include <stdexcept>
#include <string>
#include <iostream>
#include <fstream>
#include <utility>

namespace VDB {
    namespace C {
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
    }
    class Manager;
    class Database;
    class Table;
    class Cursor;
    class Schema;

    class Error : std::exception {
        C::rc_t rc;
        
    public:
        Error(C::rc_t const rc_, char const *file, int line) : rc(rc_) {
            std::cerr << "RC " << rc << " thrown by " << file << ':' << line << std::endl;
        }
        char const *what() const throw() { return ""; }
    };
    class Schema {
        friend class Manager;
        C::VSchema *const o;
        
        static C::rc_t dumpToStream(void *const p, void const *const b, size_t const s)
        {
            ((std::ostream *)p)->write((char const *)b, s);
            return 0;
        }
        void parseText(size_t const length, char const text[], char const *const name = 0)
        {
            C::rc_t const rc = VSchemaParseText(o, name, text, length);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        void addIncludePath(char const *const path)
        {
            auto const rc = VSchemaAddIncludePath(o, "%s", path);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        Schema(C::VSchema *const o_) : o(o_) {}
    public:
        Schema(Schema const &other) : o(other.o) { C::VSchemaAddRef(o); }
        ~Schema() { C::VSchemaRelease(o); }
        
        friend std::ostream &operator <<(std::ostream &strm, Schema const &s)
        {
            C::rc_t const rc = VSchemaDump(s.o, C::sdmPrint, 0, dumpToStream, (void *)&strm);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return strm;
        }
    };
    class Cursor {
        friend class Table;
        C::VCursor *const o;
        
        Cursor(C::VCursor *const o_) :o(o_) {}
    public:
        struct Data {
            unsigned elem_bits;
            unsigned elements;
            
            size_t size() const {
                return (elem_bits * elements + 7) / 8;
            }
            size_t const stride() const {
                auto const sz = ((elem_bits * elements + 7) / 8 + 3) / 4;
                return 8 + sz * 4;
            }
            void *data() const {
                return (void *)(this + 1);
            }
            void *end() const {
                return (void *)(((uint8_t const *)this) + stride());
            }
            Data const *next() const {
                return (Data const *)end();
            }
            std::string asString() const {
                if (elem_bits == 8)
                    return elements == 0 ? std::string() : std::string((char const *)data(), elements);
                else
                    throw std::logic_error("bad cast");
            }
            template <typename T> std::vector<T> asVector() const {
                if (elem_bits == sizeof(T) * 8)
                    return std::vector<T>((T *)data(), ((T *)data()) + elements);
                else
                    throw std::logic_error("bad cast");
            }
            template <typename T> T value() const {
                if (elem_bits == sizeof(T) * 8 && elements == 1)
                    return *(T *)data();
                else
                    throw std::logic_error("bad cast");
            }
        };
        struct RawData {
            void const *data;
            unsigned elem_bits;
            unsigned elements;
            
            size_t size() const {
                return (elem_bits * elements + 7) / 8;
            }
            Data const *copy(void *memory, void const *endp) const {
                auto const source = reinterpret_cast<uint8_t const *>(this->data);
                auto const rslt = (Data *)memory;
                if (rslt + 1 > endp)
                    return nullptr;
                rslt->elem_bits = elem_bits;
                rslt->elements = elements;
                if (rslt->end() > endp)
                    return nullptr;
                std::copy(source, source + size(), reinterpret_cast<uint8_t *>(rslt + 1));
                return rslt;
            }
            std::string asString() const {
                if (elem_bits == 8)
                    return elements == 0 ? std::string() : std::string((char *)data, elements);
                else
                    throw std::logic_error("bad cast");
            }
            template <typename T> std::vector<T> asVector() const {
                if (elem_bits == sizeof(T) * 8)
                    return std::vector<T>((T *)data, ((T *)data) + elements);
                else
                    throw std::logic_error("bad cast");
            }
            template <typename T> T value() const {
                if (elem_bits == sizeof(T) * 8 && elements == 1)
                    return *(T *)data;
                else
                    throw std::logic_error("bad cast");
            }
        };
        Cursor(Cursor const &other) :o(other.o) { C::VCursorAddRef(o); }
        ~Cursor() { C::VCursorRelease(o); }
        
        std::pair<int64_t, int64_t> rowRange() const
        {
            uint64_t count = 0;
            int64_t first = 0;
            C::rc_t rc = C::VCursorIdRange(o, 0, &first, &count);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return std::make_pair(first, first + count);
        }
        RawData read(int64_t row, unsigned cid) const {
            RawData out;
            void const *base = 0;
            uint32_t count = 0;
            uint32_t boff = 0;
            uint32_t elem_bits = 0;
            
            C::rc_t rc = C::VCursorCellDataDirect(o, row, cid, &elem_bits, &base, &boff, &count);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            
            out.data = base;
            out.elem_bits = elem_bits;
            out.elements = count;
            
            return out;
        }
        void read(int64_t row, unsigned const N, RawData out[]) const
        {
            for (unsigned i = 0; i < N; ++i) {
                out[i] = read(row, i + 1);
            }
        }
#if VDB_WRITEABLE
        void newRow() const {
            auto const rc = C::VCursorOpenRow(o);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        template <typename T>
        void write(unsigned col, T const &value) const {
            auto const rc = C::VCursorWrite(o, col, sizeof(T) * 8, (void const *)&value, 0, 1);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        template <typename T>
        void writeNull(unsigned col) const {
            auto dummy = T(0);
            auto const rc = C::VCursorWrite(o, col, sizeof(T) * 8, (void const *)&dummy, 0, 0);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        template <typename T>
        void write(unsigned col, unsigned N, T const *value) const {
            auto const rc = C::VCursorWrite(o, col, sizeof(T) * 8, (void const *)&value, 0, N);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        void write(unsigned col, std::string const &value) const {
            auto const rc = C::VCursorWrite(o, col, 8, (void const *)value.data(), 0, value.size());
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        void commitRow() const {
            auto rc = C::VCursorCommitRow(o);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            rc = C::VCursorCloseRow(o);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        void commit() const {
            auto const rc = C::VCursorCommit(o);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
#endif
    };
    class Table {
        friend class Database;
        C::VTable *const o;
        
        Table(C::VTable *const o_) :o(o_) {}
    public:
        Table(Table const &other) :o(other.o) { C::VTableAddRef(o); }
        ~Table() { C::VTableRelease(o); }
        
        Cursor read(unsigned const N, char const *const fields[]) const
        {
            C::VCursor const *curs = 0;
            auto rc = C::VTableCreateCursorRead(o, &curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            
            for (unsigned i = 0; i < N; ++i) {
                uint32_t cid = 0;
                
                rc = C::VCursorAddColumn(curs, &cid, "%s", fields[i]);
                if (rc) throw Error(rc, __FILE__, __LINE__);
            }
            rc = C::VCursorOpen(curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Cursor(const_cast<C::VCursor *>(curs));
        }
        
        Cursor read(std::initializer_list<char const *> const &fields) const
        {
            C::VCursor const *curs = 0;
            auto rc = C::VTableCreateCursorRead(o, &curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            
            for (auto && field : fields) {
                uint32_t cid = 0;
                
                rc = C::VCursorAddColumn(curs, &cid, "%s", field);
                if (rc) throw Error(rc, __FILE__, __LINE__);
            }
            rc = C::VCursorOpen(curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Cursor(const_cast<C::VCursor *>(curs));
        }
#if VDB_WRITEABLE
        Cursor append(unsigned const N, char const *fields[]) const
        {
            C::VCursor *curs = 0;
            auto rc = C::VTableCreateCursorWrite(o, &curs, C::kcmInsert);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            
            for (unsigned i = 0; i < N; ++i) {
                uint32_t cid = 0;
                
                rc = C::VCursorAddColumn(curs, &cid, "%s", fields[i]);
                if (rc) throw Error(rc, __FILE__, __LINE__);
                if (cid != i + 1)
                    throw std::range_error("invalid column number");
            }
            rc = C::VCursorOpen(curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Cursor(curs);
        }
#endif
    };
    class Database {
        friend class Manager;
        C::VDatabase *const o;
        
        Database(C::VDatabase *o_) :o(o_) {}
    public:
        Database(Database const &other) : o(other.o) { C::VDatabaseAddRef(o); }
        ~Database() { C::VDatabaseRelease(o); }
        
        Table operator [](std::string const &name) const
        {
            C::VTable *p = 0;
            auto const rc = C::VDatabaseOpenTableRead(o, (C::VTable const **)&p, "%s", name.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Table(p);
        }
#if VDB_WRITEABLE
        Table create(std::string const &name, std::string const &type) const
        {
            C::VTable *rslt = 0;
            auto const rc = C::VDatabaseCreateTableByMask(o, &rslt, type.c_str(), 0, 0, "%s", name.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Table(rslt);
        }

        Table open(std::string const &name) const
        {
            C::VTable *p = 0;
            auto const rc = C::VDatabaseOpenTableUpdate(o, &p, "%s", name.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Table(p);
        }
#endif
    };
    class Manager {
#if VDB_WRITEABLE
        C::VDBManager *const o;

        static C::VDBManager *makeManager() {
            C::VDBManager *o;
            auto const rc = C::VDBManagerMakeUpdate(&o, 0);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return o;
        }
#else
        C::VDBManager const *const o;
        
        static C::VDBManager const *makeManager() {
            C::VDBManager const *o;
            auto const rc = C::VDBManagerMakeRead(&o, 0);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return o;
        }
#endif
    public:
        Manager() : o(makeManager()) {}
        Manager(Manager const &other) : o(other.o) { C::VDBManagerAddRef(o); }
        ~Manager() { C::VDBManagerRelease(o); }

        Schema schema(size_t const size, char const *const text, char const *const includePath = 0) const
        {
            C::VSchema *p = 0;
            auto const rc = VDBManagerMakeSchema(o, &p);
            if (rc) throw Error(rc, __FILE__, __LINE__);

            Schema rslt(p);
            if (includePath)
                rslt.addIncludePath(includePath);
            rslt.parseText(size, text);
            return rslt;
        }
        
        Schema schemaFromFile(std::string const &path, std::string const &includePath = "") const
        {
            std::ifstream ifs(path, std::ios::ate | std::ios::in);
            if (!ifs) { throw std::runtime_error("can't open file " + path); }
            size_t size = ifs.tellg();
            char *p = new char[size];
            
            ifs.seekg(0, std::ios::beg);
            ifs.read(p, size);
            
            Schema result = schema(size, p, includePath.size() != 0 ? includePath.c_str() : 0);
            delete [] p;
            return result;
        }
#if VDB_WRITEABLE
        Database create(std::string const &path, Schema const &schema, std::string const &type) const
        {
            C::VDatabase *rslt = 0;
            auto const rc = C::VDBManagerCreateDB(o, &rslt, schema.o, type.c_str(), C::kcmInit | C::kcmMD5, "%s", path.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Database(rslt);
        }
#endif
        Database operator [](std::string const &path) const
        {
            C::VDatabase *p = 0;
            auto const rc = C::VDBManagerOpenDBRead(o, (C::VDatabase const **)&p, 0, "%s", path.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Database(p);
        }
#if VDB_WRITEABLE
        Database open(std::string const &path) const
        {
            C::VDatabase *p = 0;
            auto const rc = C::VDBManagerOpenDBUpdate(o, &p, 0, "%s", path.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Database(p);
        }
#endif
    };
}

#include <vector>
namespace utility {
    
    struct StatisticsAccumulator {
    private:
        double N;
        double sum;
        double M2;
        double min;
        double max;
    public:
        StatisticsAccumulator() : N(0) {}
        explicit StatisticsAccumulator(double const value) : sum(value), min(value), max(value), M2(0.0), N(1) {}
        
        auto count() const -> decltype(N) { return N; }
        double average() const { return sum / N; }
        double variance() const { return M2 / N; }
        double minimum() const { return min; }
        double maximum() const { return max; }

        void add(double const value) {
            if (N == 0) {
                min = max = sum = value;
                M2 = 0.0;
            }
            else {
                auto const diff = average() - value;
                
                M2 += diff * diff * N / (N + 1);
                sum += value;
                min = std::min(min, value);
                max = std::max(max, value);
            }
            N += 1;
        }
        friend StatisticsAccumulator operator +(StatisticsAccumulator a, StatisticsAccumulator b) {
            StatisticsAccumulator result;

            result.N = a.N + b.N;
            result.sum = (a.N == 0.0 ? 0.0 : a.sum) + (b.N == 0.0 ? 0.0 : b.sum);
            result.min = std::min(a.N == 0.0 ? 0.0 : a.min, b.N == 0.0 ? 0.0 : b.min);
            result.max = std::max(a.N == 0.0 ? 0.0 : a.max, b.N == 0.0 ? 0.0 : b.max);
            result.M2 = (a.N == 0.0 ? 0.0 : a.M2) + (b.N == 0.0 ? 0.0 : b.M2);
            if (a.N != 0 && b.N != 0) {
                auto const diff = a.average() - b.average();
                auto const adjust = diff * diff * a.N * b.N / result.N;
                result.M2 += adjust;
            }
            return result;
        }
    };

    static char const *programNameFromArgv0(char const *const argv0)
    {
        auto last = -1;
        for (auto i = 0; ; ++i) {
            auto const ch = argv0[i];
            if (ch == '\0') return argv0 + last + 1;
            if (ch == '/') last = i;
        }
    }
    
    struct CommandLine {
        std::string program;
        std::vector<std::string> argument;
        
        CommandLine(int const argc, char const *const *const argv)
        : argument(argv + 1, argv + argc)
        , program(programNameFromArgv0(argv[0]))
        {}
        auto arguments() const -> decltype(argument.size()) { return argument.size(); }
    };

    class strings_map {
        typedef std::vector<char> char_store_t;
        typedef unsigned index_t;
        typedef std::vector<index_t> reverse_lookup_t;
        typedef std::pair<index_t, index_t> ordered_list_elem_t;
        typedef std::vector<ordered_list_elem_t> ordered_list_t;
        
        char_store_t char_store;
        reverse_lookup_t reverse_lookup;
        ordered_list_t ordered_list;
        
        std::pair<bool, ordered_list_t::const_iterator> find(std::string const &name) const {
            auto f = ordered_list.begin();
            auto e = ordered_list.end();
            auto const base = char_store.data();
            
            while (f + 0 < e) {
                auto const m = f + ((e - f) >> 1);
                auto const cmp = name.compare(base + m->first);
                if (cmp == 0) return std::make_pair(true, m);
                if (cmp < 0)
                    e = m;
                else
                    f = m + 1;
            }
            return std::make_pair(false, f);
        }
    public:
        strings_map() {}
        strings_map(std::initializer_list<std::string> list) {
            for (auto && i : list)
                (void)operator[](i);
        }
        strings_map(std::initializer_list<char const *> list) {
            for (auto && i : list)
                (void)operator[](std::string(i));
        }
        index_t count() const {
            return (index_t)reverse_lookup.size();
        }
        bool contains(std::string const &name, index_t &id) const {
            auto const fnd = find(name);
            if (fnd.first) id = fnd.second->second;
            return fnd.first;
        }
        index_t operator[](std::string const &name) {
            auto const fnd = find(name);
            if (fnd.first) return fnd.second->second;
            
            auto const newPair = std::make_pair((index_t)char_store.size(), (index_t)ordered_list.size());
            
            char_store.insert(char_store.end(), name.begin(), name.end());
            char_store.push_back(0);
            
            ordered_list.insert(fnd.second, newPair);
            reverse_lookup.push_back(newPair.first);
            
            return (index_t)newPair.second;
        }
        std::string operator[](index_t id) const {
            if (id < reverse_lookup.size()) {
                char const *const rslt = char_store.data() + reverse_lookup[id];
                return std::string(rslt);
            }
            throw std::out_of_range("invalid id");
        }
    };
}

#endif //__VDB_HPP_INCLUDED__
