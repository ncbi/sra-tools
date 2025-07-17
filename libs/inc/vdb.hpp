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
 * ===========================================================================
 */

#ifndef __VDB_HPP_INCLUDED__
#define __VDB_HPP_INCLUDED__ 1

#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <klib/printf.h>
#include <klib/rc.h>
#include <klib/symbol.h> /* KSymbol */
#include <klib/vector.h> /* VectorForEach */

#include <kdb/manager.h>
#include <kdb/meta.h>
#include <kdb/namelist.h>
#include <vdb/cursor.h>
#include <vdb/database.h>
#include <vdb/manager.h>
#include <vdb/schema-priv.h> /* KSymbolName */
#include <vdb/schema.h>
#include <vdb/table.h>
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
        char const *file;
        int line;

    public:
        bool handled;

        ~Error() {
            if (!handled)
                std::cerr << "RC " << rc << " thrown by " << file << ':' << line << std::endl;
        }
        Error(rc_t const rc_, char const *file_, int line_)
        : rc(rc_) 
        , file(file_)
        , line(line_)
        , handled(false)
        {
            std::stringstream out;
            out << file << ":" << line << ": ";
            if ( rc != 0 )
            {
                out << ", rc = " << RcToString( rc );
            }
            text = out.str();
        }
        Error(const char * msg, char const *file_, int line_) 
        : rc(0)
        , file(file_)
        , line(line_)
        , handled(true)
        {
            std::cerr << "'" << msg << "' thrown by " << file << ':' << line << std::endl;

            std::stringstream out;
            out << file << ":" << line << ": " << msg;
            text = out.str();
        }
        Error(const std::string & msg) 
        : rc(0)
        , text(msg)
        , file(nullptr)
        , line(0)
        , handled(true)
        {
            std::cerr << msg << std::endl;
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

    class SchemaDataFormatter {
    public:
        std::string out;
        virtual void format(
            const struct SchemaData &d, int indent = -1, bool first = true) = 0;
    };

    struct SchemaData {
        typedef std::vector<SchemaData> Data;

        std::string name;
        std::vector<SchemaData> parent;

        void format(SchemaDataFormatter &v) { v.format(*this); }
    };

    struct SchemaInfo {
        SchemaData::Data db;
        SchemaData::Data table;
        SchemaData::Data view;
    };

    class Schema {
        friend class Database;
        friend class Manager;
        friend class Table;

        VSchema *const o;

        static rc_t dumpToStream(void *const p, void const *const b, size_t const s)
        {
            ((std::ostream *)p)->write((char const *)b, s);
            return 0;
        }

        static std::string MkName(KSymbolName *sn)
        {
            if (sn == NULL)
                return "";

            std::string name;

            KSymbolNameElm *e = sn->name;
            std::string part(e->name->addr, e->name->size);
            name += part;

            for (e = e->next; e; e = e->next) {
                name += ":" + std::string(e->name->addr, e->name->size);
            }

            uint32_t version = sn->version;
            char v[99] = "";
            string_printf(v, sizeof v, NULL, "#%V", version);
            name += v;

            return name;
        }

        static void OnDb(void * item, void * data)
        {
            struct SDatabase * self = static_cast<struct SDatabase *>(item);
            VDB::SchemaData::Data * sd
                = static_cast<VDB::SchemaData::Data *>(data);
            assert(self && sd);

            KSymbolName *sn = NULL;
            rc_t rc = SDatabaseMakeKSymbolName(self, &sn);

            const SDatabase * dad = NULL;
            rc_t r2 = SDatabaseGetDad(self, &dad);

            VDB::SchemaData elm;
            if (rc == 0)
                elm.name = MkName(sn);
            if (r2 == 0 && dad != NULL)
                OnDb(const_cast<SDatabase*>(dad), &elm.parent);
            sd->push_back(elm);

            KSymbolNameWhack(sn);
        }

        static void OnTbl(void * item, void * data)
        {
            struct STable * self = static_cast<struct STable *>(item);
            VDB::SchemaData::Data * sd
                = static_cast<VDB::SchemaData::Data *>(data);
            assert(self && sd);

            KSymbolName *sn = NULL;
            rc_t rc = STableMakeKSymbolName(self, &sn);

            const Vector * parents = NULL;
            rc_t r2 = STableGetParents(self, &parents);

            VDB::SchemaData elm;
            if (rc == 0)
                elm.name = MkName(sn);
            if (r2 == 0 && parents != NULL)
                VectorForEach(parents, false, OnTbl, &elm.parent);
            sd->push_back(elm);

            KSymbolNameWhack(sn);
        }

        static void OnView(void * item, void * data)
        {
            struct SView * self = static_cast<struct SView *>(item);
            VDB::SchemaData::Data * sd
                = static_cast<VDB::SchemaData::Data *>(data);
            assert(self && sd);

            KSymbolName *sn = NULL;
            rc_t rc = SViewMakeKSymbolName(self, &sn);

            const Vector * parents = NULL;
            rc_t r2 = SViewGetParents(self, &parents);

            VDB::SchemaData elm;
            if (rc == 0)
                elm.name = MkName(sn);
            if (r2 == 0 && parents != NULL)
                VectorForEach(parents, false, OnView, &elm.parent);
            sd->push_back(elm);

            KSymbolNameWhack(sn);
        }

        void parseText(size_t const length, char const text[], char const *const name = 0)
        {
            auto const rc = VSchemaParseText(o, name, text, length);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
        }
        void addIncludePath(char const *const path)
        {
            auto const rc = VSchemaAddIncludePath(o, "%s", path);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
        }
        Schema(VSchema *const o_) : o(o_) {}
    public:
        Schema(Schema const &other) : o(other.o) { VSchemaAddRef(o); }
        ~Schema() { VSchemaRelease(o); }

        VDB::SchemaInfo GetInfo(void)
        {
            VDB::SchemaInfo out;
            const Vector *v(NULL);

            VSchemaGetDb(o, &v);
            VectorForEach(v, false, OnDb, &out.db);

            VSchemaGetTbl(o, &v);
            VectorForEach(v, false, OnTbl, &out.table);

            VSchemaGetView(o, &v);
            VectorForEach(v, false, OnView, &out.view);

            return out;
        }

        friend std::ostream &operator <<(std::ostream &strm, Schema const &s)
        {
            auto const rc = VSchemaDump(s.o, sdmPrint, 0, dumpToStream, (void *)&strm);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
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
            template <typename T> T valueOr(T const &v) const {
                if (elem_bits == sizeof(T) * 8 && data != nullptr && elements == 1)
                    return *(T *)data;
                else
                    return v;
            }
        };
        Cursor(Cursor const &other) :o(other.o), N(other.N), cid(other.cid) { VCursorAddRef(o); }
        ~Cursor() { VCursorRelease(o); }
        unsigned columns() const { return N; }

        VCursor * get() { return o; }
        bool isStaticColumn( unsigned int col_idx ) const /* 0-based index in the column array used to construct this object */
        {
            bool ret = false;
            auto const rc = VCursorIsStaticColumn ( o, cid[col_idx], & ret );
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return ret;
        }

        // Are all columns in the cursor static columns?
        bool isStatic() const {
            for (unsigned i = 0; i < N; ++i) {
                if (!isStaticColumn(i))
                    return false;
            }
            return true;
        }
        std::pair<RowID, RowID> rowRange() const
        {
            uint64_t count = 0;
            int64_t first = 0;
            if (isStatic()) {
                first = 1;
                count = 1;
            }
            else {
                auto const rc = VCursorIdRange(o, 0, &first, &count);
                if (rc) throw Error{ rc, __FILE__, __LINE__ };
            }
            return std::make_pair(first, first + count);
        }
        RawData read(RowID row, unsigned int col_idx) const {
            RawData out;
            void const *base = 0;
            uint32_t count = 0;
            uint32_t boff = 0;
            uint32_t elem_bits = 0;
            auto const rc = VCursorCellDataDirect(o, row, cid[col_idx], &elem_bits, &base, &boff, &count);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };

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

    class NameList {
        KNamelist *o;

        NameList(KNamelist *const o_) : o(o_) {}
        NameList(KNamelist const *const o_) : o(const_cast<KNamelist *>(o_)) {}
    public:
        ~NameList() { KNamelistRelease(o); }
        NameList(NameList const &other) : o(other.o) { KNamelistAddRef(o); }
        NameList &operator =(NameList const &other) {
            KNamelistAddRef(other.o);
            KNamelistRelease(o);
            o = other.o;
            return *this;
        }
        template <typename P, typename FUNC>
        NameList(P p, FUNC && func) : o(nullptr) {
            auto const rc = func(p, &o);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
        }
        unsigned count() const {
            uint32_t result = 0;
            auto const rc = KNamelistCount(o, &result);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return result;
        }
        std::string operator [](unsigned index) const {
            char const *result = nullptr;
            auto const rc = KNamelistGet(o, index, &result);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return std::string{ result };
        }
        bool contains(char const *value) const {
            return KNamelistContains(o, value);
        }
        bool contains(std::string const &value) const {
            return contains(value.c_str());
        }

        operator std::vector< std::string >() const {
            auto const count = this->count();
            std::vector< std::string > result;
            result.reserve(count);
            for (unsigned i = 0; i < count; ++i)
                result.emplace_back(this->operator[](i));
            return result;
        }
    };

    class Metadata {
        friend class MetadataCollection;
        KMDataNode *o;

        Metadata(KMDataNode *const o_) : o(o_) {}
    public:
        ~Metadata() { KMDataNodeRelease(o); }
        Metadata(Metadata const &other) : o(other.o) { KMDataNodeAddRef(o); }
        Metadata &operator =(Metadata const &other)
        {
            if (o != other.o) {
                KMDataNodeAddRef(other.o);
                KMDataNodeRelease(o);
                o = other.o;
            }
            return *this;
        }

        /// @brief Get child node.
        Metadata childNode(char const *name) const {
            KMDataNode const *child = nullptr;
            auto const rc = KMDataNodeOpenNodeRead(o, &child, "%s", name);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return Metadata{ const_cast<KMDataNode *>(child) };
        }

        /// @brief Get child node.
        Metadata childNode(std::string const &name) const {
            KMDataNode const *child = nullptr;
            auto const rc = KMDataNodeOpenNodeRead(o, &child, "%.*s", (int)name.size(), name.data());
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return Metadata{ const_cast<KMDataNode *>(child) };
        }

        /// @brief Get child node.
        Metadata operator [](char const *childName) const {
            return childNode(childName);
        }

        /// @brief Get child node.
        Metadata operator [](std::string const &childName) const {
            return childNode(childName);
        }

        std::string value() const {
            std::string result(256, 0);
            do {
                size_t actual = 0;
                size_t remain = 0;
                auto const rc = KMDataNodeRead(o, 0, &result[0], result.size(), &actual, &remain);
                if (rc) throw Error{ rc, __FILE__, __LINE__ };
                result.resize(actual + remain, '\0');
                if (remain == 0)
                    return result;
            } while (1);
        }

        /// @brief Get value of attribute.
        std::string attributeValue(std::string const &attributeName) const {
            size_t size = 0;
            KMDataNodeReadAttr(o, attributeName.c_str(), nullptr, 0, &size);

            auto result = std::string(size + 1, '\0');
            auto const rc = KMDataNodeReadAttr(o, attributeName.c_str(), &result[0], result.size(), &size);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            result.resize(size);
            return result;
        }

        /// @brief Get list of child nodes.
        NameList children() const {
            return NameList{ o, KMDataNodeListChildren };
        }

        /// @brief Get list of attributes.
        NameList attributes() const {
            return NameList{ o, KMDataNodeListAttr };
        }
    };

    /// @brief aka KMetadata, i.e. the root of a metadata tree.
    class MetadataCollection {
        friend class Table;
        friend class Database;
        KMetadata *o;

        MetadataCollection(KMetadata *const o_) : o(o_) {}
        MetadataCollection(KMetadata const *const o_) : o(const_cast<KMetadata *>(o_)) {}
    public:
        ~MetadataCollection() { KMetadataRelease(o); }
        MetadataCollection(MetadataCollection const &other) : o(other.o) { KMetadataAddRef(o); }
        MetadataCollection &operator =(MetadataCollection const &other) {
            KMetadataAddRef(other.o);
            KMetadataRelease(o);
            o = other.o;
            return *this;
        }
        Metadata root() const {
            KMDataNode const *node = nullptr;
            auto const rc = KMetadataOpenNodeRead(o, &node, "");
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Metadata{ const_cast< KMDataNode * >(node) };
        }
        bool hasChildNode( const char * name ) const 
        {
            KMDataNode const *node = nullptr;
            bool ret = KMetadataOpenNodeRead(o, &node, "%s", name) == 0;
            KMDataNodeRelease( node );
            return ret;
        }
        Metadata childNode(char const *name) const {
            KMDataNode const *node = nullptr;
            auto const rc = KMetadataOpenNodeRead(o, &node, "%s", name);
            if (rc) throw Error(rc, __FILE__, __LINE__);
            return Metadata{ const_cast<KMDataNode *>(node) };
        }
        Metadata childNode(std::string const &name) const {
            return childNode(name.c_str());
        }
        Metadata operator [](char const *nodeName) const {
            return childNode(nodeName);
        }
        Metadata operator [](std::string const &nodeName) const {
            return childNode(nodeName);
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
            VCursor const *curs = 0;
            auto rc = VTableCreateCursorRead(o, &curs);
            if (rc) 
                throw Error{ rc, __FILE__, __LINE__ };

            std::vector<unsigned int> columns;
            for (unsigned i = 0; i < N; ++i) {
                uint32_t cid = 0;

                rc = VCursorAddColumn(curs, &cid, "%s", fields[i]);
                if (rc)
                {
                    VCursorRelease( curs );
                    throw Error{ rc, __FILE__, __LINE__ };
                }
                columns.push_back( cid );
            }
            rc = VCursorOpen(curs);
            if (rc)
            {
                VCursorRelease( curs );
                throw Error{ rc, __FILE__, __LINE__ };
            }
            return Cursor{ const_cast<VCursor *>(curs), columns };
        }

        Cursor read(std::initializer_list<char const *> const &fields) const
        {
            VCursor const *curs = 0;
            auto rc = VTableCreateCursorRead(o, &curs);
            if (rc)
                throw Error{ rc, __FILE__, __LINE__ };

            std::vector<unsigned int> columns;
            for (auto && field : fields) {
                uint32_t cid = 0;

                rc = VCursorAddColumn(curs, &cid, "%s", field);
                if (rc)
                {
                    VCursorRelease( curs );
                    throw Error{ rc, __FILE__, __LINE__ };
                }
                columns.push_back( cid );
            }
            rc = VCursorOpen(curs);
            if (rc)
            {
                VCursorRelease( curs );
                throw Error{ rc, __FILE__, __LINE__ };
            }
            return Cursor{ const_cast<VCursor *>(curs), columns };
        }

        Schema openSchema( void ) const
        {
            const VSchema *schema = NULL;
            rc_t rc = VTableOpenSchema( o, &schema );
            if (rc != 0)
                throw Error{ rc, __FILE__, __LINE__ };
            return Schema(const_cast<VSchema *>(schema));
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

        MetadataCollection metadata() const {
            KMetadata const *meta = nullptr;
            auto const rc = VTableOpenMetadataRead(o, &meta);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return MetadataCollection{ const_cast< KMetadata * >(meta) };
        }

    private:
        ColumnNames listColumns(  rc_t CC listfn ( struct VTable const *self, struct KNamelist **names ) ) const
        {
            return NameList{ o, listfn };
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
            VTable const *p = 0;
            auto const rc = VDatabaseOpenTableRead(o, &p, "%s", name.c_str());
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return Table { const_cast<VTable *>(p) };
        }

        bool hasTable( const std::string & table ) const
        {
            KNamelist *names;
            rc_t rc = VDatabaseListTbl ( o, & names );
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            bool ret = KNamelistContains( names, table.c_str() );
            KNamelistRelease( names );
            return ret;
        }

        Schema openSchema( void ) const
        {
            const VSchema *schema = NULL;
            rc_t rc = VDatabaseOpenSchema( o, &schema );
            if (rc != 0)
                throw Error{ rc, __FILE__, __LINE__ };
            return Schema { const_cast<VSchema *>(schema) };
        }

        MetadataCollection metadata() const {
            KMetadata const *meta = nullptr;
            auto const rc = VDatabaseOpenMetadataRead(o, &meta);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return MetadataCollection { const_cast< KMetadata * >(meta) };
        }
    };

    class Manager {
        VDBManager const *const o;

        static VDBManager const *makeManager() {
            VDBManager const *o;
            auto const rc = VDBManagerMakeRead(&o, 0);
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
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
            if (rc) throw Error{ rc, __FILE__, __LINE__ };

            auto rslt = Schema{ p };
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
            VDatabase const *p = 0;
            auto const rc = VDBManagerOpenDBRead(o, &p, 0, "%s", path.c_str());
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return Database{ const_cast<VDatabase *>(p) };
        }
        Table openTable(std::string const &path) const
        {
            VTable const *p = 0;
            auto const rc = VDBManagerOpenTableRead(o, &p, 0, "%s", path.c_str());
            if (rc) throw Error{ rc, __FILE__, __LINE__ };
            return Table{ const_cast<VTable *>(p) };
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
