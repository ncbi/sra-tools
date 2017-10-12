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
        std::ostream &out;
        
        class StreamHeader {
            friend std::ostream &operator <<(std::ostream &os, StreamHeader const &self)
            {
                struct h {
                    char sig[8];
                    uint32_t endian;
                    uint32_t version;
                    uint32_t size;
                    uint32_t packing;
                } const h = { { 'N', 'C', 'B', 'I', 'g', 'n', 'l', 'd' }, 1, 2, sizeof(struct h), 0 };
                return os.write(&h.sig[0], sizeof(h));
            }
        public:
            StreamHeader() {};
        };
        
        class SimpleEvent {
            uint32_t eid;

            friend std::ostream &operator <<(std::ostream &os, SimpleEvent const &self)
            {
                return os.write((char const *)&self.eid, sizeof(self.eid));
            }
        public:
            SimpleEvent(EventCode const code, unsigned const id) : eid((code << 24) + id) {}
        };
        
        class String1Event {
            uint32_t eid;
            std::string const &str;

            friend std::ostream &operator <<(std::ostream &os, String1Event const &self)
            {
                uint32_t const zero = 0;
                auto const size = (uint32_t)self.str.size();
                auto const padding = (4 - (size & 3)) & 3;
                return os.write((char const *)&self.eid, sizeof(self.eid))
                         .write((char const *)&size, sizeof(size))
                         .write(self.str.data(), size)
                         .write((char const *)&zero, padding);
            }
        public:
            String1Event(EventCode const code, unsigned const id, std::string const &str_)
            : eid((code << 24) + id)
            , str(str_)
            {}
        };
        
        class String2Event {
            uint32_t eid;
            std::string const &str1;
            std::string const &str2;

            friend std::ostream &operator <<(std::ostream &os, String2Event const &self)
            {
                uint32_t const zero = 0;
                auto const size1 = (uint32_t)self.str1.size();
                auto const size2 = (uint32_t)self.str2.size();
                auto const size = size1 + size2;
                auto const padding = (4 - (size & 3)) & 3;
                return os.write((char const *)&self.eid, sizeof(self.eid))
                         .write((char const *)&size1, sizeof(size1))
                         .write((char const *)&size2, sizeof(size2))
                         .write(self.str1.data(), size1)
                         .write(self.str2.data(), size2)
                         .write((char const *)&zero, padding);
            }
        public:
            String2Event(EventCode const code, unsigned const id, std::string const &str_1, std::string const &str_2)
            : eid((code << 24) + id)
            , str1(str_1)
            , str2(str_2)
            {}
        };
        
        class ColumnEvent {
            uint32_t eid;
            uint32_t tid;
            uint32_t bits;
            std::string const &name;
            
            friend std::ostream &operator <<(std::ostream &os, ColumnEvent const &self)
            {
                uint32_t const zero = 0;
                auto const size = (uint32_t)self.name.size();
                auto const padding = (4 - (size & 3)) & 3;
                return os.write((char const *)&self.eid, sizeof(self.eid))
                         .write((char const *)&self.tid, sizeof(self.tid))
                         .write((char const *)&self.bits, sizeof(self.bits))
                         .write((char const *)&size, sizeof(size))
                         .write(self.name.data(), size)
                         .write((char const *)&zero, padding);
            }
        public:
            ColumnEvent(EventCode const code, unsigned const cid, unsigned const tid_, unsigned const elemBits, std::string const &str)
            : eid((code << 24) + cid)
            , tid(tid_)
            , bits(elemBits)
            , name(str)
            {}
        };

        std::ostream &write(EventCode const code, unsigned const cid, uint32_t const count, uint32_t const elsize, void const *data) const
        {
            uint32_t const eid = (code << 24) + cid;
            uint32_t const zero = 0;
            auto const size = elsize * count;
            auto const padding = (4 - (size & 3)) & 3;
            return out.write((char const *)&eid, sizeof(eid))
                      .write((char const *)&count, sizeof(count))
                      .write((char const *)data, size)
                      .write((char const *)&zero, padding);
        }
        template <typename T>
        std::ostream &write(EventCode const code, unsigned const cid, uint32_t const count, T const *data) const
        {
            uint32_t const eid = (code << 24) + cid;
            uint32_t const zero = 0;
            auto const size = sizeof(T) * count;
            auto const padding = (4 - (size & 3)) & 3;
            return out.write((char const *)&eid, sizeof(eid))
                      .write((char const *)&count, sizeof(count))
                      .write((char const *)data, size)
                      .write((char const *)&zero, padding);
        }
        template <typename T>
        std::ostream &write(EventCode const code, unsigned const cid, T const &data) const
        {
            return write(code, cid, 1, &data);
        }
        std::ostream &write(EventCode const code, unsigned const cid, std::string const &data) const
        {
            return write(code, cid, (uint32_t)data.size(), data.data());
        }
    public:
        Writer(std::ostream &out_)
        : out(out_)
        {
            out << StreamHeader();
        }

        std::ostream &errorMessage(std::string const &message) const
        {
            return out << String1Event(errMessage, 0, message);
        }
        
        std::ostream &destination(std::string const &remoteDb) const
        {
            return out << String1Event(remotePath, 0, remoteDb);
        }
        
        std::ostream &schema(std::string const &file, std::string const &dbSpec) const
        {
            return out << String2Event(useSchema, 0, file, dbSpec);
        }
        
        std::ostream &info(std::string const &name, std::string const &version) const
        {
            return out << String2Event(writerName, 0, name, version);
        }
        
        std::ostream &openTable(unsigned const tid, std::string const &name) const
        {
            return out << String1Event(newTable, tid, name);
        }
        
        std::ostream &openColumn(unsigned const cid, unsigned const tid, unsigned const elemBits, std::string const &colSpec) const
        {
            return out << ColumnEvent(newColumn, cid, tid, elemBits, colSpec);
        }
        
        std::ostream &beginWriting() const
        {
            return out << SimpleEvent(openStream, 0);
        }
        
        template <typename T>
        std::ostream &defaultValue(unsigned const cid, uint32_t const count, T const *data) const
        {
            return write(cellDefault, cid, count, data);
        }
        template <typename T>
        std::ostream &defaultValue(unsigned const cid, T const &data) const
        {
            return write(cellDefault, cid, 1, &data);
        }
        std::ostream &defaultValue(unsigned const cid, std::string const &data) const
        {
            return write(cellDefault, cid, data);
        }
        
        std::ostream &value(unsigned const cid, uint32_t const count, uint32_t const elsize, void const *data) const
        {
            return write(cellData, cid, count, elsize, data);
        }
        template <typename T>
        std::ostream &value(unsigned const cid, uint32_t const count, T const *data) const
        {
            return write(cellData, cid, count, data);
        }
        template <typename T>
        std::ostream &value(unsigned const cid, T const &data) const
        {
            return write(cellData, cid, 1, &data);
        }
        std::ostream &value(unsigned const cid, std::string const &data) const
        {
            return write(cellData, cid, data);
        }
        
        std::ostream &closeRow(unsigned const tid) const
        {
            return out << SimpleEvent(nextRow, tid);
        }
        
        enum MetaNodeRoot {
            database, table, column
        };
        std::ostream &setMetadata(MetaNodeRoot const root, unsigned const oid, std::string const &name, std::string const &value) const
        {
            auto const code = root == database ? dbMeta
                            : root == table    ? tableMeta
                            : root == column   ? columnMeta
                            : badEvent;
            return out << String2Event(code, oid, name, value);
        }
        
        std::ostream &endWriting() const
        {
            return out << SimpleEvent(endStream, 0);
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
    using VDB::Writer::endWriting;
    using VDB::Writer::setMetadata;
    
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
        Table(Writer2 const &p, Writer2::Tables::const_iterator n) : parent(p), t(n), table(t->second.first) {}
    public:
        Column column(std::string const &column) const
        {
            auto const &columns = t->second.second;
            auto const c = columns.find(column);
            if (c == columns.end())
                throw std::logic_error(column + " is not a column of table " + t->first);
            return Column(parent, c->second);
        }
        auto closeRow() const -> decltype(parent.closeRow(0)) {
            return parent.closeRow(table);
        }
    };

    class Column {
        friend Writer2::Table;
        Writer2::ColumnID columnNumber;
        Writer2 const &parent;
        Column(Writer2 const &p, Writer2::ColumnID n) : parent(p), columnNumber(n) {}
    public:
        template <typename T>
        auto setValue(T const &data) const -> decltype(parent.value(0, T(0))) {
            return parent.value(columnNumber, data);
        }
        template <typename T>
        auto setValue(unsigned count, T const *data) const -> decltype(parent.value(0, 0, data)) {
            return parent.value(columnNumber, uint32_t(count), data);
        }
        auto setValue(std::string const &data) const -> decltype(parent.value(0, std::string())) {
            return parent.value(columnNumber, data);
        }
        auto setValueEmpty() const -> decltype(parent.value(0, 0, "")) {
            return parent.value(columnNumber, 0, "");
        }
        template <typename T>
        auto setDefault(T const &data) const -> decltype(parent.defaultValue(0, T(0))) {
            return parent.defaultValue(columnNumber, data);
        }
        template <typename T>
        auto setDefault(unsigned count, T const *data) const -> decltype(parent.defaultValue(0, 0, data)) {
            return parent.defaultValue(columnNumber, uint32_t(count), data);
        }
        auto setDefault(std::string const &data) const -> decltype(parent.defaultValue(0, std::string())) {
            return parent.defaultValue(columnNumber, data);
        }
        auto setDefaultEmpty() const -> decltype(parent.defaultValue(0, 0, "")) {
            return parent.defaultValue(columnNumber, 0, "");
        }
    };

    Table table(std::string const &table) const {
        auto const t = tables.find(table);
        if (t == tables.end())
            throw std::logic_error(table + " is not the name of a table");
        return Table(*this, t);
    }
    
    Writer2(std::ostream &os)
    : VDB::Writer(os)
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
};

#endif // __WRITER_HPP_INCLUDED__
