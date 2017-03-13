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

#include <stdexcept>
#include <string>
#include <iostream>
#include <fstream>

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
        Error(C::rc_t const rc_) : rc(rc_) {}
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
        void parseText(unsigned length, char const text[], char const *const name = 0)
        {
            C::rc_t const rc = VSchemaParseText(o, name, text, length);
            if (rc) throw Error(rc);
        }
        Schema(C::VSchema *const o_) : o(o_) {}
    public:
        Schema(Schema const &other) : o(other.o) { C::VSchemaAddRef(o); }
        ~Schema() { C::VSchemaRelease(o); }
        
        friend std::ostream &operator <<(std::ostream &strm, Schema const &s)
        {
            C::rc_t const rc = VSchemaDump(s.o, C::sdmPrint, 0, dumpToStream, (void *)&strm);
            if (rc) throw Error(rc);
            return strm;
        }
    };
    class Table {
        friend class Database;
        C::VTable *const o;
        
        Table(C::VTable *const o_) :o(o_) {}
    public:
        Table(Table const &other) :o(other.o) { C::VTableAddRef(o); }
        ~Table() { C::VTableRelease(o); }
    };
    class Database {
        friend class Manager;
        C::VDatabase *const o;
        
        Database(C::VDatabase *o_) :o(o_) {}
    public:
        Database(Database const &other) : o(other.o) { C::VDatabaseAddRef(o); }
        ~Database() { C::VDatabaseRelease(o); }
        
        Table create(std::string const &name, std::string const &type) const
        {
            C::VTable *rslt = 0;
            C::rc_t const rc = C::VDatabaseCreateTableByMask(o, &rslt, type.c_str(), 0, 0, "%s", name.c_str());
            if (rc) throw Error(rc);
            return Table(rslt);
        }
        
        Table operator [](std::string const &name) const
        {
            C::VTable *p = 0;
            C::rc_t const rc = C::VDatabaseOpenTableRead(o, (C::VTable const **)&p, "%s", name.c_str());
            if (rc) throw Error(rc);
            return Table(p);
        }

        Table open(std::string const &name) const
        {
            C::VTable *p = 0;
            C::rc_t const rc = C::VDatabaseOpenTableUpdate(o, &p, "%s", name.c_str());
            if (rc) throw Error(rc);
            return Table(p);
        }
    };
    class Manager {
        C::VDBManager *o;

    public:
        Manager() : o(0) {
            C::rc_t const rc = C::VDBManagerMakeUpdate(&o, 0);
            if (rc) throw Error(rc);
        }
        Manager(Manager const &other) : o(other.o) { C::VDBManagerAddRef(o); }
        ~Manager() { C::VDBManagerRelease(o); }

        Schema schema(unsigned const size, char const *const text) const
        {
            C::VSchema *p = 0;
            C::rc_t const rc = VDBManagerMakeSchema(o, &p);
            if (rc) throw Error(rc);

            Schema rslt(p);
            rslt.parseText(size, text);
            return rslt;
        }
        
        Schema schemaFromFile(std::string const &path) const
        {
            std::ifstream ifs(path, std::ios::ate | std::ios::in);
            if (!ifs) { throw std::runtime_error("can't open file " + path); }
            size_t size = ifs.tellg();
            char *p = new char[size];
            
            ifs.seekg(0, std::ios::beg);
            ifs.read(p, size);
            
            Schema result = schema(size, p);
            delete [] p;
            return result;
        }
        
        Database create(std::string const &path, Schema const &schema, std::string const &type) const
        {
            C::VDatabase *rslt = 0;
            C::rc_t const rc = C::VDBManagerCreateDB(o, &rslt, (C::VSchema *)&schema, type.c_str(), C::kcmInit | C::kcmMD5, "%s", path.c_str());
            if (rc) throw Error(rc);
            return Database(rslt);
        }
        
        Database operator [](std::string const &path) const
        {
            C::VDatabase *p = 0;
            C::rc_t const rc = C::VDBManagerOpenDBRead(o, (C::VDatabase const **)&p, 0, "%s", path.c_str());
            if (rc) throw Error(rc);
            return Database(p);
        }
        
        Database open(std::string const &path) const
        {
            C::VDatabase *p = 0;
            C::rc_t const rc = C::VDBManagerOpenDBUpdate(o, &p, 0, "%s", path.c_str());
            if (rc) throw Error(rc);
            return Database(p);
        }
    };
}
