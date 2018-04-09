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

#include "vdb.hpp"

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
            columnMeta
        };
        FILE *stream;
        
        class StreamHeader {
            friend Writer;
            bool write(FILE *const stream) const
            {
                struct h {
                    char sig[8];
                    uint32_t endian;
                    uint32_t version;
                    uint32_t size;
                    uint32_t packing;
                } const h = { { 'N', 'C', 'B', 'I', 'g', 'n', 'l', 'd' }, 1, 2, sizeof(struct h), 0 };
                return fwrite(&h, sizeof(h), 1, stream) == 1;
            }
        public:
            StreamHeader() {};
        };
        
        class SimpleEvent {
            friend Writer;
            uint32_t eid;

            bool write(FILE *const stream) const
            {
                return fwrite(&eid, sizeof(eid), 1, stream) == 1;
            }
        public:
            SimpleEvent(EventCode const code, unsigned const id) : eid((code << 24) + id) {}
        };
        
        class String1Event {
            friend Writer;
            uint32_t eid;
            std::string const &str;

            bool write(FILE *const stream) const {
                uint32_t const zero = 0;
                auto const size = (uint32_t)str.size();
                auto const padding = (4 - (size & 3)) & 3;
                return fwrite(&eid, sizeof(eid), 1, stream) == 1
                    && fwrite(&size, sizeof(size), 1, stream) == 1
                    && fwrite(str.data(), 1, size, stream) == size
                    && fwrite(&zero, 1, padding, stream) == padding;
            }
        public:
            String1Event(EventCode const code, unsigned const id, std::string const &str_)
            : eid((code << 24) + id)
            , str(str_)
            {}
        };
        
        class String2Event {
            friend Writer;
            uint32_t eid;
            std::string const &str1;
            std::string const &str2;

            bool write(FILE *const stream) const {
                uint32_t const zero = 0;
                auto const size1 = (uint32_t)str1.size();
                auto const size2 = (uint32_t)str2.size();
                auto const size = size1 + size2;
                auto const padding = (4 - (size & 3)) & 3;
                return fwrite(&eid, sizeof(eid), 1, stream) == 1
                    && fwrite(&size1, sizeof(size1), 1, stream) == 1
                    && fwrite(&size2, sizeof(size2), 1, stream) == 1
                    && fwrite(str1.data(), 1, size1, stream) == size1
                    && fwrite(str2.data(), 1, size2, stream) == size2
                    && fwrite(&zero, 1, padding, stream) == padding;
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
            
            bool write(FILE *const stream) const {
                uint32_t const zero = 0;
                auto const size = (uint32_t)name.size();
                auto const padding = (4 - (size & 3)) & 3;
                return fwrite(&eid, sizeof(eid), 1, stream) == 1
                    && fwrite(&tid, sizeof(tid), 1, stream) == 1
                    && fwrite(&bits, sizeof(bits), 1, stream) == 1
                    && fwrite(&size, sizeof(size), 1, stream) == 1
                    && fwrite(name.data(), 1, size, stream) == size
                    && fwrite(&zero, 1, padding, stream) == padding;
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
            uint32_t const eid = (code << 24) + cid;
            uint32_t const zero = 0;
            auto const size = elsize * count;
            auto const padding = (4 - (size & 3)) & 3;
            return fwrite(&eid, sizeof(eid), 1, stream) == 1
                && fwrite(&count, sizeof(count), 1, stream) == 1
                && fwrite(data, elsize, count, stream) == count
                && fwrite(&zero, 1, padding, stream) == padding;
        }
        template <typename T>
        bool write(EventCode const code, unsigned const cid, uint32_t const count, T const *data) const
        {
            uint32_t const eid = (code << 24) + cid;
            uint32_t const zero = 0;
            auto const size = sizeof(T) * count;
            auto const padding = (4 - (size & 3)) & 3;
            return fwrite(&eid, sizeof(eid), 1, stream) == 1
                && fwrite(&count, sizeof(count), 1, stream) == 1
                && fwrite(data, sizeof(T), count, stream) == count
                && fwrite(&zero, 1, padding, stream) == padding;
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
        Writer(FILE *const stream_)
        : stream(stream_)
        {
            StreamHeader().write(stream);
        }

        bool errorMessage(std::string const &message) const
        {
            return String1Event(errMessage, 0, message).write(stream);
        }
        
        bool destination(std::string const &remoteDb) const
        {
            return String1Event(remotePath, 0, remoteDb).write(stream);
        }
        
        bool schema(std::string const &file, std::string const &dbSpec) const
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
        
        auto flush() const -> decltype(fflush(stream)) {
            return fflush(stream);
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

    struct ColumnDefinition {
        char const *name;
        int elemSize;
    };
    
    class Column;
    class Table {
        friend Writer2;
        Writer2 const &parent;
        Writer2::TableID table;
        Writer2::Tables::const_iterator const t;
        Table(Writer2 const &p, Writer2::Tables::const_iterator n) : parent(p), t(n) {
            table = t->second.first;
        }
    public:
        Column column(std::string const &column) const
        {
            auto const &columns = t->second.second;
            auto const c = columns.find(column);
            if (c == columns.end())
                throw std::logic_error(column + " is not a column of table " + t->first);
            return Column(parent, c->second);
        }
        bool closeRow() const {
            return parent.closeRow(table);
        }
    };

    class Column {
        friend Writer2::Table;
        Writer2::ColumnID columnNumber;
        Writer2 const &parent;
        Column(Writer2 const &p, Writer2::ColumnID n) : parent(p), columnNumber(n) {}
    public:
        bool setValue(VDB::Cursor::Data const *data) const {
            return setValue(data->elements, data->elem_bits >> 3, data->data());
        }
        bool setValue(VDB::Cursor::DataList const *data) const {
            return setValue(static_cast<VDB::Cursor::Data const *>(data));
        }
        template <typename T>
        bool setValue(T const &data) const {
            return parent.value(columnNumber, data);
        }
        template <typename T>
        bool setValue(unsigned count, T const *data) const {
            return parent.value(columnNumber, uint32_t(count), data);
        }
        bool setValue(unsigned count, unsigned elsize, void const *data) const {
            return parent.value(columnNumber, count, elsize, data);
        }
        bool setValueEmpty() const {
            return parent.value(columnNumber, 0, "");
        }
        template <typename T>
        bool setDefault(T const &data) const {
            return parent.defaultValue(columnNumber, data);
        }
        template <typename T>
        bool setDefault(unsigned count, T const *data) const {
            return parent.defaultValue(columnNumber, uint32_t(count), data);
        }
        bool setDefault(std::string const &data) const {
            return parent.defaultValue(columnNumber, data);
        }
        bool setDefaultEmpty() const {
            return parent.defaultValue(columnNumber, 0, "");
        }
    };

    Table table(std::string const &table) const {
        auto const t = tables.find(table);
        if (t == tables.end())
            throw std::logic_error(table + " is not the name of a table");
        return Table(*this, t);
    }
    
    Writer2(FILE *const stream)
    : VDB::Writer(stream)
    , nextTable(0)
    , nextColumn(0)
    {
    }
    void addTable(char const *name, std::initializer_list<ColumnDefinition> list)
    {
        decltype(tables.begin()->second.second) columns;
        auto tno = ++nextTable;
        this->openTable(tno, name);
        for (auto && i : list) {
            auto cno = ++nextColumn;
            this->openColumn(cno, tno, i.elemSize * 8, i.name);
            columns[i.name] = cno;
        }
        tables[name] = std::make_pair(tno, columns);
    }
    bool setValue(ColumnID columnNumber, unsigned count, unsigned elsize, void const *data) const {
        return value(columnNumber, count, elsize, data);
    }
    bool setValue(ColumnID columnNumber, VDB::Cursor::Data const *data) const {
        return setValue(columnNumber, data->elements, data->elem_bits >> 3, data->data());
    }
};


#endif // __WRITER_HPP_INCLUDED__
