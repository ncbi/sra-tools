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
#include <sstream>
#include <memory>
// #include <utility>
// #include <map>
#include <vector>
// #include <algorithm>

#include <klib/rc.h>
#include <klib/printf.h>

#include <kdb/manager.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
#include <vdb/vdb-priv.h>

namespace VDB {
    class Manager;
    class Database;
    class Table;
    class Cursor;
    class Schema;

    class Error : public std::exception {
        rc_t rc;
        std::string text;

    public:
        Error(rc_t const rc_, char const *file, int line) : rc(rc_) {
            std::cerr << "RC " << rc << " thrown by " << file << ':' << line << std::endl;

            std::stringstream out;
            out << file << ":" << line << ": ";
            if ( rc != 0 )
            {
                out << ", rc = " << RcToString( rc );
            }
            text = out.str();
        }
        Error(const char * msg, char const *file, int line) : rc(0) {
            std::cerr << "'" << msg << "' thrown by " << file << ':' << line << std::endl;

            std::stringstream out;
            out << file << ":" << line << ": " << msg;
            text = out.str();
        }
        Error(const std::string & msg) : rc(0) {
            std::cerr << msg << std::endl;

            std::stringstream out;
            out << msg;
            text = out.str();
        }

        char const *what() const throw()
        {
            return text.c_str();
        }
        rc_t getRc() const { return rc; }

        static std::string RcToString( rc_t rc, bool english = false )
        {
            // obtain the required size
            size_t num_writ;
            const char* FmtRc = english ? "%#R" : "%R";
            string_printf ( nullptr, 0, &num_writ, FmtRc, rc );

            std::string ret( num_writ, ' ' );
            string_printf ( &ret[0], num_writ, nullptr, FmtRc, rc );
            return ret;
        }
    };

    class Schema {
        friend class Manager;
        VSchema *const o;

        static rc_t dumpToStream(void *const p, void const *const b, size_t const s)
        {
            ((std::ostream *)p)->write((char const *)b, s);
            return 0;
        }
        void parseText(size_t const length, char const text[], char const *const name = 0)
        {
            rc_t const rc = VSchemaParseText(o, name, text, length);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        void addIncludePath(char const *const path)
        {
            auto const rc = VSchemaAddIncludePath(o, "%s", path);
            if (rc) throw Error(rc, __FILE__, __LINE__);
        }
        Schema(VSchema *const o_) : o(o_) {}
    public:
        Schema(Schema const &other) : o(other.o) { VSchemaAddRef(o); }
        ~Schema() { VSchemaRelease(o); }

        friend std::ostream &operator <<(std::ostream &strm, Schema const &s)
        {
            rc_t const rc = VSchemaDump(s.o, sdmPrint, 0, dumpToStream, (void *)&strm);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return strm;
        }
    };
    class Cursor {
        friend class Table;
        VCursor *const o;
    protected:
        unsigned const N;
        std::vector<unsigned int> cid;

        Cursor(VCursor *const o_, std::vector<unsigned int> columns)
        : o(o_), N((unsigned)columns.size()), cid(columns)
        {
        }

    public:
        using RowID = int64_t;

        struct RawData {
            void const *data;
            unsigned elem_bits;
            unsigned elements;

            size_t size() const {
                return (elem_bits * elements + 7) / 8;
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
        Cursor(Cursor const &other) :o(other.o), N(other.N), cid(other.cid) { VCursorAddRef(o); }
        ~Cursor() { VCursorRelease(o); }
        unsigned columns() const { return N; }

        VCursor * get() { return o; }
        bool isStaticColumn( unsigned int col_idx ) const /* 0-based index in the column array used to construct this object */
        {
            bool ret;
            rc_t rc = VCursorIsStaticColumn ( o, cid[col_idx], & ret );
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return ret;
        }

        std::pair<RowID, RowID> rowRange() const
        {
            uint64_t count = 0;
            int64_t first = 0;
            rc_t rc = VCursorIdRange(o, 0, &first, &count);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return std::make_pair(first, first + count);
        }
        RawData read(RowID row, unsigned int col_idx) const {
            RawData out;
            void const *base = 0;
            uint32_t count = 0;
            uint32_t boff = 0;
            uint32_t elem_bits = 0;
            rc_t rc = VCursorCellDataDirect(o, row, cid[col_idx], &elem_bits, &base, &boff, &count);
            if (rc) throw Error(rc, __FILE__, __LINE__);

            out.data = base;
            out.elem_bits = elem_bits;
            out.elements = count;

            return out;
        }
        void read(RowID row, unsigned const n, RawData out[]) const
        {
            for (unsigned i = 0; i < n; ++i) {
                out[i] = read(row, i);
            }
        }
        template <typename F>
        uint64_t foreach(F f) const {
            auto data = std::vector<RawData>();
            auto const range = rowRange();
            uint64_t rows = 0;

            data.resize(N);
            for (auto i = range.first; i < range.second; ++i) {
                for (unsigned j = 0; j < N; ++j) {
                    try { data[j] = read(i, j); }
                    catch (...) { return rows; }
                }
                f(i, data);
                ++rows;
            }
            return rows;
        }
        template <typename FILT, typename FUNC>
        uint64_t foreach(FILT filt, FUNC func) const {
            auto data = std::vector<RawData>();
            auto const range = rowRange();
            uint64_t rows = 0;

            data.resize(N);
            for (auto i = range.first; i < range.second; ++i) {
                auto const keep = filt(*this, i);
                data.clear();
                if (keep) {
                    for (unsigned j = 0; j < N; ++j) {
                        try { data.push_back( read(i, j) ); }
                        catch (...) { return rows; }
                    }
                }
                func(i, keep, data);
                ++rows;
            }
            return rows;
        }
    };

    class Table {
        friend class Database;
        friend class Manager;
        VTable * o;

        Table(VTable *const o_) :o(o_) {}
    public:
        Table() : o(nullptr) {}
        Table(Table const &other) :o(other.o) { VTableAddRef(o); }
        ~Table() { VTableRelease(o); }

        Table& operator=( const Table & other )
        {
            if ( this != &other )
            {   // the following calls will not throw
                VTableAddRef(other.o);
                VTableRelease(o);
                o = other.o;
            }
            return *this;
        }

        Cursor read(unsigned const N, char const *const fields[]) const
        {
            unsigned n = 0;
            VCursor const *curs = 0;
            auto rc = VTableCreateCursorRead(o, &curs);
            if (rc) throw Error(rc, __FILE__, __LINE__);

            std::vector<unsigned int> columns;
            for (unsigned i = 0; i < N; ++i) {
                uint32_t cid = 0;

                rc = VCursorAddColumn(curs, &cid, "%s", fields[i]);
                if (rc)
                {
                    VCursorRelease( curs );
                    throw Error(rc, __FILE__, __LINE__);
                }
                columns.push_back( cid );
                ++n;
            }
            rc = VCursorOpen(curs);
            if (rc)
            {
                VCursorRelease( curs );
                throw Error(rc, __FILE__, __LINE__);
            }
            return Cursor(const_cast<VCursor *>(curs), columns);
        }

        Cursor read(std::initializer_list<char const *> const &fields) const
        {
            unsigned n = 0;
            VCursor const *curs = 0;
            auto rc = VTableCreateCursorRead(o, &curs);
            if (rc)
            {
                VCursorRelease( curs );
                throw Error(rc, __FILE__, __LINE__);
            }

            std::vector<unsigned int> columns;
            for (auto && field : fields) {
                uint32_t cid = 0;

                rc = VCursorAddColumn(curs, &cid, "%s", field);
                if (rc)
                {
                    VCursorRelease( curs );
                    throw Error(rc, __FILE__, __LINE__);
                }
                columns.push_back( cid );
                ++n;
            }
            rc = VCursorOpen(curs);
            if (rc)
            {
                VCursorRelease( curs );
                throw Error(rc, __FILE__, __LINE__);
            }
            return Cursor(const_cast<VCursor *>(curs), columns);
        }

        typedef std::vector< std::string > ColumnNames;
        ColumnNames physicalColumns() const
        {
            return listColumns( VTableListPhysColumns );
        }
        ColumnNames readableColumns() const
        {
            return listColumns( VTableListReadableColumns );
        }

    private:
        ColumnNames listColumns(  rc_t CC listfn ( struct VTable const *self, struct KNamelist **names ) ) const
        {
            KNamelist *names;
            rc_t rc = listfn ( o, & names );
            if (rc)
            {
                throw Error(rc, __FILE__, __LINE__);
            }
            uint32_t count;
            rc = KNamelistCount ( names, &count );
            if (rc)
            {
                KNamelistRelease ( names );
                throw Error(rc, __FILE__, __LINE__);
            }
            ColumnNames ret;
            for ( uint32_t i = 0; i < count; ++i )
            {
                const char * name;
                rc = KNamelistGet ( names, i, & name );
                if (rc)
                {
                    KNamelistRelease ( names );
                    throw Error(rc, __FILE__, __LINE__);
                }
                ret.push_back( name );
            }
            KNamelistRelease ( names );
            return ret;
        }


    };

    class Database {
        friend class Manager;
        VDatabase *const o;

        Database(VDatabase *o_) :o(o_) {}
    public:
        Database(Database const &other) : o(other.o) { VDatabaseAddRef(o); }
        ~Database() { VDatabaseRelease(o); }

        Table operator [](std::string const &name) const
        {
            VTable *p = 0;
            auto const rc = VDatabaseOpenTableRead(o, (VTable const **)&p, "%s", name.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Table(p);
        }

        bool hasTable( const std::string & table ) const
        {
            KNamelist *names;
            rc_t rc = VDatabaseListTbl ( o, & names );
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return KNamelistContains( names, table.c_str() );
        }
    };

    class Manager {
        VDBManager const *const o;

        static VDBManager const *makeManager() {
            VDBManager const *o;
            auto const rc = VDBManagerMakeRead(&o, 0);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return o;
        }
    public:
        Manager() : o(makeManager()) {}
        Manager(Manager const &other) : o(other.o) { VDBManagerAddRef(o); }
        ~Manager() { VDBManagerRelease(o); }

        Schema schema(size_t const size, char const *const text, char const *const includePath = 0) const
        {
            VSchema *p = 0;
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

        Database openDatabase(std::string const &path) const
        {
            VDatabase *p = 0;
            auto const rc = VDBManagerOpenDBRead(o, (VDatabase const **)&p, 0, "%s", path.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Database(p);
        }
        Table openTable(std::string const &path) const
        {
            VTable *p = 0;
            auto const rc = VDBManagerOpenTableRead(o, (VTable const **)&p, 0, "%s", path.c_str());
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Table(p);
        }

        typedef enum {
            ptDatabase,
            ptTable,
            ptPrereleaseTable,
            ptInvalid
        } PathType;
        PathType pathType( const std::string & path ) const
        {
            switch ( VDBManagerPathType( o, "%s", path.c_str() ) & ~ kptAlias )
            {
                case kptDatabase :      return ptDatabase;
                case kptTable :         return ptTable;
                case kptPrereleaseTbl : return ptPrereleaseTable;
                default :               return ptInvalid;
            }
        }

    };
}

#endif //__VDB_HPP_INCLUDED__
