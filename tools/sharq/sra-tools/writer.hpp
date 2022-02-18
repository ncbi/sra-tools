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

#ifndef __WRITER_HPP_INCLUDED__
#define __WRITER_HPP_INCLUDED__ 1

#include <stdexcept>
#include <string>
#include <iostream>
#include <utility>
#include <cstdint>
#include <algorithm>
#include <cstdio>

namespace VDB {
    class Writer {
        enum EventCode {
            badEvent = 0,
            errMessage,
            endStream,

            remotePath,
            useSchema,
            newTable,
            newColumn,
            openStream,
            cellDefault,
            cellData,
            nextRow,

            moveAhead,
            errMessage2,
            remotePath2,
            useSchema2,
            newTable2,
            cellDflt2,
            cellData2,
            emptyDflt,

            writerName,

            dbMeta,
            tableMeta,
            columnMeta,

            dbMeta2,
            tableMeta2,
            columnMeta2,

            addMbrDb, // ???
            addMbrTbl, // ???

            logMesg,
            progressMesg
        };
        ostream& stream;
        ///FILE *stream;

        class StreamHeader {
            friend Writer;
            bool write(ostream& stream) const
            {
                struct h {
                    char sig[8];
                    uint32_t endian;
                    uint32_t version;
                    uint32_t size;
                    uint32_t packing;
                } const h = { { 'N', 'C', 'B', 'I', 'g', 'n', 'l', 'd' }, 1, 2, sizeof(struct h), 0 };
                //return fwrite(&h, sizeof(h), 1, stream) == 1;
                stream.write((const char*)&h, sizeof(h));
                return true;
            }
        public:
            StreamHeader() {};
        };

        class SimpleEvent {
            friend Writer;
            uint32_t eid;
            bool write(ostream& stream) const
            {
                stream.write((const char*)&eid, sizeof(eid));
                return true;
            }
        public:
            SimpleEvent(EventCode const code, unsigned const id) : eid((code << 24) + id) {}
        };

        class String1Event {
            friend Writer;
            uint32_t eid;
            std::string const &str;
            bool write(ostream& stream) const {
                uint32_t const zero = 0;
                auto const size = (uint32_t)str.size();
                auto const padding = (4 - (size & 3)) & 3;
                stream.write((const char*)&eid, sizeof(eid));
                stream.write((const char*)&size, sizeof(size));
                stream.write((const char*)str.data(), size);
                stream.write((const char*)&zero, padding);
                return true;
            }

        public:
            String1Event(EventCode const code, unsigned const id, std::string const &str)
            : eid((code << 24) + id)
            , str(str)
            {}
        };

        class String2Event {
            friend Writer;
            uint32_t eid;
            std::string const &str1;
            std::string const &str2;
            bool write(ostream& stream) const {
                uint32_t const zero = 0;
                auto const size1 = (uint32_t)str1.size();
                auto const size2 = (uint32_t)str2.size();
                auto const size = size1 + size2;
                auto const padding = (4 - (size & 3)) & 3;
                stream.write((const char*)&eid, sizeof(eid));
                stream.write((const char*)&size1, sizeof(size1));
                stream.write((const char*)&size2, sizeof(size2));
                stream.write((const char*)str1.data(), size1);
                stream.write((const char*)str2.data(), size2);
                stream.write((const char*)&zero, padding);
                return true;
            }

        public:
            String2Event(EventCode const code, unsigned const id, std::string const &str_1, std::string const &str_2)
            : eid((code << 24) + id)
            , str1(str_1)
            , str2(str_2)
            {}
        };

        class ColumnEvent {
            friend Writer;
            uint32_t eid;
            uint32_t tid;
            uint32_t bits;
            std::string const &name;
            bool write(ostream& stream) const {
                uint32_t const zero = 0;
                auto const size = (uint32_t)name.size();
                auto const padding = (4 - (size & 3)) & 3;
                stream.write((const char*)&eid, sizeof(eid));
                stream.write((const char*)&tid, sizeof(tid));
                stream.write((const char*)&bits, sizeof(bits));
                stream.write((const char*)&size, sizeof(size));
                stream.write((const char*)name.data(), size);
                stream.write((const char*)&zero, padding);
                return true;
            }

        public:
            ColumnEvent(EventCode const code, unsigned const cid, unsigned const tid_, unsigned const elemBits, std::string const &str)
            : eid((code << 24) + cid)
            , tid(tid_)
            , bits(elemBits)
            , name(str)
            {}
        };

        bool write(EventCode const code, unsigned const cid, uint32_t const count, uint32_t const elsize, void const *data) const
        {
            if ((int)cid == -1)
                return true;
            uint32_t const eid = (code << 24) + cid;
            uint32_t const zero = 0;
            auto const size = elsize * count;
            auto const padding = (4 - (size & 3)) & 3;
            stream.write((const char*)&eid, sizeof(eid));
            stream.write((const char*)&count, sizeof(count));
            stream.write((const char*)data, elsize * count);
            stream.write((const char*)&zero, padding);
            return true;
        }
        template <typename T>
        bool write(EventCode const code, unsigned const cid, uint32_t const count, T const *data) const
        {
            if ((int)cid == -1)
                return true;
            uint32_t const eid = (code << 24) + cid;
            uint32_t const zero = 0;
            auto const size = sizeof(T) * count;
            auto const padding = (4 - (size & 3)) & 3;
            stream.write((const char*)&eid, sizeof(eid));
            stream.write((const char*)&count, sizeof(count));
            stream.write((const char*)data, sizeof(T) * count);
            stream.write((const char*)&zero, padding);
            return true;
        }

        template <typename T>
        bool write(EventCode const code, unsigned const cid, T const &data) const
        {
            return write(code, cid, 1, &data);
        }
        bool write(EventCode const code, unsigned const cid, std::string const &data) const
        {
            return write(code, cid, (uint32_t)data.size(), (uint32_t)sizeof(std::string::value_type), data.data());
        }
    public:
        Writer(ostream& stream_)
        : stream(stream_)
        {
            StreamHeader().write(stream);
        }

        bool logMessage(std::string const &message) const
        {
            return String1Event(logMesg, 0, message).write(stream);
        }

        bool progressMessage(std::string const &message) const
        {
            return String1Event(progressMesg, 0, message).write(stream);
        }

        bool errorMessage(std::string const &message) const
        {
            return String1Event(errMessage, 0, message).write(stream);
        }

        virtual bool destination(std::string const &remoteDb) const
        {
            return String1Event(remotePath, 0, remoteDb).write(stream);
        }

        virtual bool schema(std::string const &file, std::string const &dbSpec) const
        {
            return String2Event(useSchema, 0, file, dbSpec).write(stream);
        }

        bool info(std::string const &name, std::string const &version) const
        {
            return String2Event(writerName, 0, name, version).write(stream);
        }

        bool openTable(unsigned const tid, std::string const &name) const
        {
            return String1Event(newTable, tid, name).write(stream);
        }

        bool openColumn(unsigned const cid, unsigned const tid, unsigned const elemBits, std::string const &colSpec) const
        {
            return ColumnEvent(newColumn, cid, tid, elemBits, colSpec).write(stream);
        }

        bool beginWriting() const
        {
            return SimpleEvent(openStream, 0).write(stream);
        }

        template <typename T>
        bool defaultValue(unsigned const cid, uint32_t const count, T const *data) const
        {
            return write(cellDefault, cid, count, data);
        }
        template <typename T>
        bool defaultValue(unsigned const cid, T const &data) const
        {
            return write(cellDefault, cid, 1, &data);
        }
        bool defaultValue(unsigned const cid, std::string const &data) const
        {
            return write(cellDefault, cid, data);
        }

        bool value(unsigned const cid, uint32_t const count, uint32_t const elsize, void const *data) const
        {
            return write(cellData, cid, count, elsize, data);
        }
        template <typename T>
        bool value(unsigned const cid, uint32_t const count, T const *data) const
        {
            return write(cellData, cid, count, data);
        }
        template <typename T>
        bool value(unsigned const cid, T const &data) const
        {
            return write(cellData, cid, 1, &data);
        }
        bool value(unsigned const cid, std::string const &data) const
        {
            return write(cellData, cid, data);
        }

        bool closeRow(unsigned const tid) const
        {
            return SimpleEvent(nextRow, tid).write(stream);
        }

        enum MetaNodeRoot {
            database, table, column
        };
        bool setMetadata(MetaNodeRoot const root, unsigned const oid, std::string const &name, std::string const &value) const
        {
            auto const code = root == database ? dbMeta
                            : root == table    ? tableMeta
                            : root == column   ? columnMeta
                            : badEvent;
            return String2Event(code, oid, name, value).write(stream);
        }

        bool endWriting() const
        {
            return SimpleEvent(endStream, 0).write(stream);
        }
        void flush() const {
            stream.flush();
        }

    };
}

#include <map>
class Writer2 : private VDB::Writer {
public:
    typedef int ColumnID, TableID;
    typedef std::map<std::string, ColumnID> Columns;
    typedef std::pair<TableID, Columns> TableEntry;
    typedef std::map<std::string, TableEntry> Tables;
private:
    TableID nextTable;
    ColumnID nextColumn;
    Tables tables;
public:
    using VDB::Writer::destination;
    using VDB::Writer::schema;
    using VDB::Writer::info;
    using VDB::Writer::beginWriting;
    using VDB::Writer::closeRow;
    using VDB::Writer::setMetadata;
    using VDB::Writer::endWriting;
    using VDB::Writer::flush;
    using VDB::Writer::errorMessage;
    using VDB::Writer::logMessage;
    using VDB::Writer::progressMessage;

    struct ColumnDefinition {
        char const *name;
        char const *expr;
        int elemSize;

        ColumnDefinition(char const *const name, int const elemSize, char const *expr = nullptr)
        : name(name)
        , expr(expr == nullptr ? name : expr)
        , elemSize(elemSize)
        {}
    };

    class Column;
    class Table {
        friend Writer2;
        Writer2 const *parent;
        Writer2::TableEntry const *table_entry;
        string table_name;
        Table(Writer2 const &p, const string& name) : parent(&p), table_name(name) {
            auto const t = p.tables.find(name);
            if (t == p.tables.end())
                throw std::logic_error(name + " is not the name of a table");
            table_entry = &t->second;
        }
    public:
        Table() {}

        Column column(std::string const &column) const
        {
            auto const &columns = table_entry->second;
            auto const c = columns.find(column);
            if (c == columns.end())
                throw std::logic_error(column + " is not a column of table " + table_name);
            return Column(parent, c->second);
        }
        bool closeRow() const {
            return parent->closeRow(table_entry->first);
        }
    };

    class Column {
        friend Writer2::Table;
        Writer2 const *parent;
        Writer2::ColumnID columnNumber;
        Column(Writer2 const *p, Writer2::ColumnID n) : parent(p), columnNumber(n) {}
    public:
        Column() {}

#if __VDB_HPP_INCLUDED__
        bool setValue(VDB::Cursor::Data const *data) const {
            return setValue(data->elements, data->elem_bits >> 3, data->data());
        }
        bool setValue(VDB::Cursor::DataList const *data) const {
            return setValue(static_cast<VDB::Cursor::Data const *>(data));
        }
#endif
        template <typename T>
        bool setValue(T const &data) const {
            return parent->value(columnNumber, data);
        }
        template <typename T>
        bool setValue(unsigned count, T const *data) const {
            return parent->value(columnNumber, uint32_t(count), data);
        }
        bool setValue(unsigned count, unsigned elsize, void const *data) const {
            return parent->value(columnNumber, count, elsize, data);
        }
        bool setValueEmpty() const {
            return parent->value(columnNumber, 0, "");
        }
        template <typename T>
        bool setDefault(T const &data) const {
            return parent->defaultValue(columnNumber, data);
        }
        template <typename T>
        bool setDefault(unsigned count, T const *data) const {
            return parent->defaultValue(columnNumber, uint32_t(count), data);
        }
        bool setDefault(std::string const &data) const {
            return parent->defaultValue(columnNumber, data);
        }
        bool setDefaultEmpty() const {
            return parent->defaultValue(columnNumber, 0, "");
        }
    };

    Table table(std::string const &table) const {
        return Table(*this, table);
    }

    Writer2(ostream& stream)
    : VDB::Writer(stream)
    , nextTable(0)
    , nextColumn(0)
    {
    }
    void addTable(char const *name, std::vector<ColumnDefinition> const &list)
    {
        decltype(tables.begin()->second.second) columns;
        auto const tableNo = ++nextTable;
        openTable(tableNo, name);
        for (auto && i : list) {
            if (i.expr && strcmp(i.expr, "NONE") == 0) {
                columns[i.name] = -1;
            } else {
                auto const columnNo = ++nextColumn;
                openColumn(columnNo, tableNo, i.elemSize * 8, i.expr);
                columns[i.name] = columnNo;
            }
        }
        tables[name] = std::make_pair(tableNo, columns);
    }
    bool setValue(ColumnID columnNumber, unsigned count, unsigned elsize, void const *data) const {
        return value(columnNumber, count, elsize, data);
    }
#if __VDB_HPP_INCLUDED__
    bool setValue(ColumnID columnNumber, VDB::Cursor::Data const *data) const {
        return setValue(columnNumber, data->elements, data->elem_bits >> 3, data->data());
    }
#endif
};


#endif // __WRITER_HPP_INCLUDED__
