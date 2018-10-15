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

static_assert(sizeof(unsigned) >= sizeof(uint32_t), "unsigned int is too small!");

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
        char const *what() const noexcept { return ""; }
    };
    class Schema {
        friend class Manager;
        C::VSchema *const o;
        
        static C::rc_t dumpToStream(void *const p, void const *const b, size_t const s) noexcept
        {
            auto &strm = *reinterpret_cast<std::ostream *>(p);
            auto const string = reinterpret_cast<char const *>(b);
            strm.write(string, s);
            return 0;
        }
        void parseText(size_t const length, char const text[], char const *const name = 0)
        {
            auto const rc = VSchemaParseText(o, name, text, length);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        void addIncludePath(char const *const path)
        {
            auto const rc = VSchemaAddIncludePath(o, "%s", path);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        Schema(C::VSchema *const o_) : o(o_) {}
    public:
        Schema(Schema const &other) noexcept : o(other.o) { C::VSchemaAddRef(o); }
        ~Schema() noexcept { C::VSchemaRelease(o); }
        
        friend std::ostream &operator <<(std::ostream &strm, Schema const &s)
        {
            auto const rc = VSchemaDump(s.o, C::sdmPrint, 0, dumpToStream, (void *)&strm);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return strm;
        }
    };
    class Cursor {
        friend class Table;
        C::VCursor *const o;
    protected:
        unsigned const N;
        
        Cursor(C::VCursor *const o_, unsigned columns_) noexcept :o(o_), N(columns_) {}
    public:
        using RowID = int64_t;
        struct RowRange : private std::pair<RowID, RowID> {
            RowRange(RowID const beg, RowID const end) : std::pair<RowID, RowID>(std::make_pair(beg, end)) {
                if (end < beg)
                    throw std::range_error("RowRange");
            }
            
            RowID beg() const noexcept { return first; }
            RowID end() const noexcept { return second; }
            uint64_t count() const noexcept { return second - first; }
        };
        struct Data {
            unsigned elem_bits;
            unsigned elements;
            
            /// \brief size of data in bytes
            size_t size() const noexcept {
                return (elem_bits * elements + 7) >> 3;
            }
            static int constexpr alignment() noexcept { return 4; }
            /// \brief offset of data
            static size_t constexpr offset() noexcept {
                return ((sizeof(Data) + 15) >> alignment()) << alignment();
            }
            void *data() const noexcept {
                return (void *)(reinterpret_cast<uint8_t const *>(this) + offset());
            }
            std::string string() const {
                if (elem_bits != 8 * sizeof(std::string::value_type))
                    throw std::bad_cast();
                return elements == 0 ? std::string() : std::string(reinterpret_cast<char const *>(data()), elements);
            }
            template <typename T> std::vector<T> vector() const {
                if (elem_bits != sizeof(T) * 8)
                    throw std::bad_cast();

                auto result = std::vector<T>(elements);
                std::memmove(result.data(), data(), elements * sizeof(T));
                return result;
            }
            template <typename T> T value() const {
                if (elem_bits != sizeof(T) * 8 || elements != 1)
                    throw std::bad_cast();

                T result;
                std::memmove(&result, data(), sizeof(T));
                return result;
            }
        };
        struct DataList : public Data {
            size_t stride() const noexcept {
                return offset() + (((size() + 15) >> alignment()) << alignment());
            }
            void *end() const noexcept {
                return (void *)(((char const *)this) + stride());
            }
            DataList const *next() const noexcept {
                return (DataList const *)end();
            }
        };
        struct RawData {
            void const *data;
            unsigned elem_bits;
            unsigned elements;
            
            size_t size() const noexcept {
                return (elem_bits * elements + 7) / 8;
            }
            DataList const *copy(void *memory, void const *endp) const noexcept {
                if (endp <= memory) return nullptr;

                auto const avail = (uint8_t *)endp - (uint8_t *)memory;
                if (avail < sizeof(DataList)) return nullptr;
                    
                auto const rslt = (DataList *)memory;
                rslt->elem_bits = elem_bits;
                rslt->elements = elements;
                
                if (avail < rslt->stride()) return nullptr;
                std::memmove(rslt->data(), this->data, size());
                return rslt;
            }
            template <typename T> bool equal(T beg, T const end) const {
                if (elem_bits != sizeof(*beg) * 8)
                    return false;

                auto *cp = reinterpret_cast<typename T::value_type const *>(data);
                auto const np = cp + elements;
                while (cp != np && beg != end) {
                    if (*cp++ != *beg++) return false;
                }
                return cp == np && beg == end;
            }
            std::string string() const {
                if (elem_bits == 8)
                    return elements == 0 ? std::string() : std::string((char *)data, elements);
                else
                    throw std::logic_error("bad cast");
            }
            template <typename T> std::vector<T> vector() const {
                if (elem_bits == sizeof(T) * 8)
                    return std::vector<T>((T *)data, ((T *)data) + elements);
                else
                    throw std::logic_error("bad cast");
            }
            template <typename T> T value(unsigned index = 0) const {
                if (elem_bits == sizeof(T) * 8 && index < elements)
                    return ((T *)data)[index];
                else if (index >= elements)
                    throw std::out_of_range("RawData");
                else
                    throw std::logic_error("bad cast");
            }
        };
        Cursor(Cursor const &other) :o(other.o), N(other.N) { C::VCursorAddRef(o); }
        ~Cursor() { C::VCursorRelease(o); }
        unsigned columns() const { return N; }
        
        RowRange rowRange() const
        {
            uint64_t count = 0;
            int64_t first = 0;
            C::rc_t rc = C::VCursorIdRange(o, 0, &first, &count);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return RowRange(first, first + count);
        }
        RawData read(RowID row, unsigned cid) const {
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
        void read(RowID row, unsigned const N, RawData out[]) const
        {
            for (unsigned i = 0; i < N; ++i) {
                out[i] = read(row, i + 1);
            }
        }
        template <typename F>
        uint64_t foreach(F &&f) const {
            auto data = std::vector<RawData>();
            auto const range = rowRange();
            uint64_t rows = 0;
            
            data.resize(N);
            for (auto row = range.beg(); row < range.end(); ++row) {
                for (auto i = 0; i < N; ++i) {
                    try { data[i] = read(row, i + 1); }
                    catch (...) { return rows; }
                }
                f(row, data);
                ++rows;
            }
            return rows;
        }
        template <typename FILT, typename FUNC>
        uint64_t foreach(FILT &&filt, FUNC &&func) const {
            auto data = std::vector<RawData>();
            auto const range = rowRange();
            uint64_t rows = 0;
            
            data.resize(N);
            for (auto row = range.beg(); row < range.end(); ++row) {
                auto const keep = filt(*this, row);
                if (keep) {
                    for (auto i = 0; i < N; ++i) {
                        try { data[i] = read(row, i + 1); }
                        catch (...) { return rows; }
                    }
                }
                func(row, keep, data);
                data.clear();
                ++rows;
            }
            return rows;
        }
        void *save(RowID const row, void *const dst, void const *const end) const {
            auto out = dst;
            for (auto i = 0; i < N; ++i) {
                auto const data = read(row, i + 1);
                auto const p = data.copy(out, end);
                if (p == nullptr)
                    return nullptr;
                out = p->end();
            }
            return out;
        }
    };
    class IndexedCursorBase : public Cursor {
    protected:
        using RowMap = std::map<RowID, unsigned>;
        void *const buffer;
        void const *const bufEnd;
        
        void *save(RowID row, void *at) const {
            return Cursor::save(row, (void *)at, (void *)bufEnd);
        }

        IndexedCursorBase(Cursor const &curs, size_t bufSize)
        : Cursor(curs)
        , buffer(malloc(bufSize ? bufSize : defaultBufferSize()))
        , bufEnd(((char *)buffer) + (bufSize ? bufSize : defaultBufferSize()))
        {
            if (buffer == nullptr) throw std::bad_alloc();
        }
        ~IndexedCursorBase() {
            free(buffer);
        }
        RowMap loadBuffer(std::vector<RowID> const &rows, unsigned &numread, size_t &bytesused) const {
            auto result = RowMap();
            auto cur = buffer;
            for (auto && row : rows) {
                auto const tmp = Cursor::save(row, cur, bufEnd);
                if (tmp) {
                    auto const bytes = (uint8_t const *)tmp - (uint8_t const *)cur;
                    cur = tmp;
                    result[row] = unsigned(bytesused >> VDB::Cursor::Data::alignment());
                    bytesused += bytes;
                    ++numread;
                    assert((bytesused & ((1 << VDB::Cursor::Data::alignment()) - 1)) == 0);
                }
                else
                    break;
            }
            return result;
        }
    public:
        struct Record {
            Cursor::DataList const *data;
            RowID row;
            
            Record(void const *const data, RowID row)
            : data((Cursor::DataList const *)data)
            , row(row)
            {}
        };

        size_t bufferSize() const { return (char *)bufEnd - (char *)buffer; }
        static size_t defaultBufferSize() {
            return 4ull * 1024ull * 1024ull * 1024ull;
        }
        RawData read(RowID row, unsigned cid) const = delete;
        void read(RowID row, unsigned const N, RawData out[]) const = delete;
    protected:
        Record getRecord(RowMap const &rowmap, RowID const row) const {
            auto const i = rowmap.find(row); assert(i != rowmap.end());
            auto const offset = i->second << VDB::Cursor::Data::alignment();
            return Record((void const *)((uint8_t const *)buffer + offset), row);
        }
    };
    
    /*
     * The requirements on T are
     *  T(IndexedCursorBase::Record const &)
     *  typename T::IndexIteratorT
     *  typename T::IndexIteratorT::const_reference (this typename is not used directly, see operator *())
     *  T::IndexIteratorT(T::IndexIteratorT const &) or T::IndexIteratorT::operator =
     *  T::IndexIteratorT::operator !=
     *  T::IndexIteratorT::operator <
     *  T::IndexIteratorT::operator -
     *  T::IndexIteratorT::operator ++
     *  T::IndexIteratorT::const_reference T::IndexIteratorT::operator *() (const_reference is implied to be whatever this returns)
     *  Cursor::RowID T::IndexIteratorT::const_reference::row()
     */
    template <typename T>
    class IndexedCursor : public IndexedCursorBase {
        using IndexIteratorT = typename T::IndexIteratorT;
        IndexIteratorT beg;
        IndexIteratorT const end;
        
        RowMap loadBlock(IndexIteratorT const i, IndexIteratorT const j, unsigned &count, size_t &used) const {
            auto v = std::vector<RowID>();
            
            v.reserve(std::min(j, end) - i);
            if (j < end) {
                for (auto ii = i; ii < j; ++ii)
                    v.emplace_back((*ii).row());
            }
            else {
                for (auto ii = i; ii < end; ++ii)
                    v.emplace_back((*ii).row());
            }
            std::sort(v.begin(), v.end());

            return loadBuffer(v, count, used);
        }
    public:
        IndexedCursor(Cursor const &p, IndexIteratorT index, IndexIteratorT indexEnd, size_t bufSize = 0)
        : IndexedCursorBase(p, bufSize)
        , beg(index)
        , end(indexEnd)
        {}
        template <typename F>
        uint64_t foreach(F f) const {
            auto firstTime = true;
            auto blockSize = std::min(size_t(1000000), size_t((end - beg) / 10));
            auto rows = uint64_t(0);
            
            for (auto i = beg; i != end; ) {
                unsigned count = 0;
                size_t used = 0;
                auto const block = loadBlock(i, i + blockSize, count, used);
                auto const j = i + block.size();
                while (i < j) {
                    auto const a = (*i++).row();
                    T const &rec = getRecord(block, a);
                    f(rec);
                    ++rows;
                }
                if (firstTime) {
                    blockSize = 0.95 * bufferSize() * count / used;
                    firstTime = false;
                }
            }
            return rows;
        }
    };
    
    /*
     * Specialized to deal with cases where there could be collision between
     * index keys, i.e. two records with the same key that shouldn't be equal.
     * This can happen with hash indexes.
     *
     * This version is less efficient, and could potentially have to backtrack.
     *
     * The requirements on T are same as above, with the addition of:
     *  T::operator <
     *  T::IndexIteratorT::const_reference::operator ==
     */
    template <typename T>
    class CollidableIndexedCursor : public IndexedCursorBase {
        using IndexIteratorT = typename T::IndexIteratorT;
        IndexIteratorT beg;
        IndexIteratorT const end;
        
        RowMap loadBlock(IndexIteratorT const i, IndexIteratorT const j, unsigned &count, size_t &used) const {
            auto v = std::vector<RowID>();

            v.reserve(std::min(j, end) - i);
            if (j < end) {
                auto n = v.size();
                auto l = i;
                
                for (auto ii = i; ii < j; ++ii) {
                    if (!(*l == *ii)) {
                        l = ii;
                        n = v.size();
                    }
                    v.emplace_back((*ii).row());
                }
                v.resize(n);
            }
            else {
                for (auto ii = i; ii < end; ++ii)
                    v.emplace_back((*ii).row());
            }
            std::sort(v.begin(), v.end());
            
            auto result = loadBuffer(v, count, used);
            if (count != v.size())
                result.clear();
            return result;
        }
    public:
        CollidableIndexedCursor(Cursor const &p, IndexIteratorT index, IndexIteratorT indexEnd, size_t bufSize = 0)
        : IndexedCursorBase(p, bufSize)
        , beg(index)
        , end(indexEnd)
        {}
        template <typename F>
        uint64_t foreach(F &&f) const {
            auto firstTime = true;
            auto blockSize = std::min(size_t(1000000), size_t((end - beg) / 10));
            auto rowset = std::vector<RowID>();
            auto rows = uint64_t(0);

            for (auto i = beg; i != end; ) {
                unsigned count = 0;
                size_t used = 0;
                auto const block = loadBlock(i, i + blockSize, count, used);
                auto const j = i + block.size();
                while (i < j) {
                    auto const &k = *i;
                    do { rowset.emplace_back((*i++).row()); } while (i < j && *i == k);
                    
                    std::sort(rowset.begin(), rowset.end(), [&](RowID a, RowID b) {
                        T const &A = getRecord(block, a);
                        T const &B = getRecord(block, b);
                        return A < B;
                    });
                    for (auto && a : rowset) {
                        T const &rec = getRecord(block, a);
                        f(rec);
                        ++rows;
                    }
                    rowset.clear();
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
                auto const name = fields[i];
                
                rc = C::VCursorAddColumn(curs, &cid, "%s", name);
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
    };
    class Manager {
        C::VDBManager const *const o;
        
        static C::VDBManager const *makeManager() {
            C::VDBManager const *o;
            auto const rc = C::VDBManagerMakeRead(&o, 0);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return o;
        }
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
        Database operator [](std::string const &path) const
        {
            C::VDatabase *p = 0;
            auto const rc = C::VDBManagerOpenDBRead(o, (C::VDatabase const **)&p, 0, "%s", path.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Database(p);
        }
    };
}

#endif //__VDB_HPP_INCLUDED__
