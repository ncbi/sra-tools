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
#include <map>

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
    protected:
        unsigned const N;
        
        Cursor(C::VCursor *const o_, unsigned columns_) :o(o_), N(columns_) {}
    public:
        struct Data {
            unsigned elem_bits;
            unsigned elements;
            
            size_t size() const {
                return (elem_bits * elements + 7) / 8;
            }
            void *data() const {
                return (void *)(this + 1);
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
        struct DataList : public Data {
            size_t stride() const {
                auto const sz = ((elem_bits * elements + 7) / 8 + 3) / 4;
                return 8 + sz * 4;
            }
            void *end() const {
                return (void *)(((char const *)this) + stride());
            }
            DataList const *next() const {
                return (DataList const *)end();
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
                auto const source = (char const *)(this->data);
                auto const rslt = (DataList *)memory;
                if (rslt + 1 > endp)
                    return nullptr;
                rslt->elem_bits = elem_bits;
                rslt->elements = elements;
                if (rslt->end() > endp)
                    return nullptr;
                std::copy(source, source + size(), (char *)(rslt + 1));
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
        Cursor(Cursor const &other) :o(other.o), N(other.N) { C::VCursorAddRef(o); }
        ~Cursor() { C::VCursorRelease(o); }
        unsigned columns() const { return N; }
        
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
        template <typename F>
        uint64_t foreach(F f) const {
            auto data = std::vector<RawData>();
            auto const range = rowRange();
            uint64_t rows = 0;
            
            data.reserve(N);
            for (auto i = range.first; i < range.second; ++i) {
                for (auto j = 0; j < N; ++j) {
                    void const *base = 0;
                    uint32_t count = 0;
                    uint32_t boff = 0;
                    uint32_t elem_bits = 0;
                    
                    C::rc_t rc = C::VCursorCellDataDirect(o, i, j + 1, &elem_bits, &base, &boff, &count);
                    if (rc) return rows;
                    data[j].data = base;
                    data[j].elements = count;
                    data[j].elem_bits = elem_bits;
                }
                f(i, data);
                ++rows;
            }
            return rows;
        }
        void *save(int64_t const row, void *const dst, void const *const end) const {
            auto out = dst;
            for (auto i = 0; i < N; ++i) {
                auto const data = read(row, i + 1);
                auto p = static_cast<VDB::Cursor::DataList const *>(data.copy(out, end));
                if (p == nullptr)
                    return nullptr;
                out = p->end();
            }
            return out;
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
    class IndexedCursorBase : public Cursor {
    protected:
        char *const buffer;
        char const *const bufEnd;

    public:
        struct Record {
            int64_t row;
            Cursor::DataList const *data;
            
            Record(void const *const data, int64_t row) : row(row), data((Cursor::DataList const *)data) {}
        };

        IndexedCursorBase(Cursor const &curs, size_t bufSize)
        : Cursor(curs)
        , buffer((char *)malloc(bufSize))
        , bufEnd(buffer + bufSize)
        {
            if (buffer == nullptr) throw std::bad_alloc();
        }
        ~IndexedCursorBase() {
            free(buffer);
        }
        size_t bufferSize() const { return bufEnd - buffer; }
    };
    template <typename T>
    class IndexedCursor : public IndexedCursorBase {
        typedef typename T::IndexIteratorT IndexIteratorT;
        IndexIteratorT beg;
        IndexIteratorT const end;
        
        std::map<int64_t, unsigned> loadBlock(IndexIteratorT const i, IndexIteratorT const j, unsigned &count, size_t &used) const {
            auto map = std::map<int64_t, unsigned>();
            auto v = std::vector<int64_t>();

            if (j >= end) {
                v.reserve(end - i);
                for (auto ii = i; ii < end; ++ii)
                    v.emplace_back(ii->row());
            }
            else {
                auto n = v.size();
                auto l = i;

                v.reserve(j - i);
                for (auto ii = i; ii < j; ++ii) {
                    if (!(*l == *ii)) {
                        l = ii;
                        n = v.size();
                    }
                    v.emplace_back(ii->row());
                }
                v.resize(n);
            }
            std::sort(v.begin(), v.end());
            
            auto cur = buffer;
            for (auto && row : v) {
                assert((used & 0x3) == 0);
                map[row] = unsigned(used >> 2);
                cur = (char *)save(row, (void *)cur, (void const *)bufEnd);
                if (cur == nullptr) {
                    map.clear();
                    break;
                }
                used = cur - buffer;
                ++count;
            }
            return map;
        }
    public:
        IndexedCursor(Cursor const &p, IndexIteratorT index, IndexIteratorT indexEnd, size_t bufSize = 4ul * 1024ul * 1024ul * 1024ul)
        : IndexedCursorBase(p, bufSize)
        , beg(index)
        , end(indexEnd)
        {}
        template <typename F>
        uint64_t foreach(F f) const {
            auto firstTime = true;
            auto blockSize = std::min(size_t(1000000), size_t((end - beg) / 10));
            auto rowset = std::vector<int64_t>();
            auto rows = uint64_t(0);

            for (auto i = beg; i != end; ) {
                unsigned count = 0;
                size_t used = 0;
                auto const block = loadBlock(i, i + blockSize, count, used);
                auto const j = i + block.size();
                while (i < j) {
                    rowset.clear();
                    
                    auto const &k = *i;
                    do { rowset.emplace_back(i++->row()); } while (i < j && *i == k);
                    
                    std::sort(rowset.begin(), rowset.end(), [&](uint64_t a, uint64_t b) {
                        auto const aa = block.find(a); assert(aa != block.end());
                        auto const bb = block.find(b); assert(bb != block.end());
                        T const &A = Record((void const *)(buffer + (aa->second << 2)), a);
                        T const &B = Record((void const *)(buffer + (bb->second << 2)), b);
                        return A < B;
                    });
                    for (auto && a : rowset) {
                        auto const aa = block.find(a); assert(aa != block.end());
                        T const &A = Record((void const *)(buffer + (aa->second << 2)), a);
                        f(A);
                        ++rows;
                    }
                }
                if (block.empty())
                    blockSize = 0.99 * count;
                else if (firstTime) {
                    blockSize = 0.95 * bufferSize() * count / used;
                    firstTime = false;
                }
            }
            return rows;
        }
        RawData read(int64_t row, unsigned cid) const = delete;
        void read(int64_t row, unsigned const N, RawData out[]) const = delete;
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
            unsigned n = 0;
            C::VCursor const *curs = 0;
            auto rc = C::VTableCreateCursorRead(o, &curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            
            for (unsigned i = 0; i < N; ++i) {
                uint32_t cid = 0;
                
                rc = C::VCursorAddColumn(curs, &cid, "%s", fields[i]);
                if (rc) throw Error(rc, __FILE__, __LINE__);
                ++n;
            }
            rc = C::VCursorOpen(curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Cursor(const_cast<C::VCursor *>(curs), n);
        }
        
        Cursor read(std::initializer_list<char const *> const &fields) const
        {
            unsigned n = 0;
            C::VCursor const *curs = 0;
            auto rc = C::VTableCreateCursorRead(o, &curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            
            for (auto && field : fields) {
                uint32_t cid = 0;
                
                rc = C::VCursorAddColumn(curs, &cid, "%s", field);
                if (rc) throw Error(rc, __FILE__, __LINE__);
                ++n;
            }
            rc = C::VCursorOpen(curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Cursor(const_cast<C::VCursor *>(curs), n);
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

#endif //__VDB_HPP_INCLUDED__
