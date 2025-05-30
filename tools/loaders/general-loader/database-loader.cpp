/*===========================================================================
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
 */

#include "general-loader.hpp"

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/time.h>

#include <kdb/meta.h>
#include <kdb/table.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/cursor.h>
#include <vdb/table.h>
#include <vdb/vdb-priv.h>

#include <kapp/main.h>

#include <loader/loader-meta.h>

#include <algorithm>

using namespace std;

///////////// GeneralLoader::DatabaseLoader

GeneralLoader :: DatabaseLoader :: DatabaseLoader ( const std::string&  p_programName,
                                                    const Paths&        p_includePaths,
                                                    const Paths&        p_schemas,
                                                    const std::string&  p_dbNameOverride )
:   m_includePaths ( p_includePaths ),
    m_schemas ( p_schemas ),
    m_programName ( p_programName ),
    m_databaseName ( p_dbNameOverride ), // if specified, overrides the database path coming in from the stream
    m_softwareVersion ( 0 ),
    m_mgr ( 0 ),
    m_schema ( 0 ),
    m_databaseNameOverridden ( ! m_databaseName.empty() )
{
    m_databases . insert ( Databases :: value_type ( 0, (VDatabase*)0 ) ); // reserve root database
}

GeneralLoader :: DatabaseLoader :: ~DatabaseLoader ()
{
    m_tables . clear();
    m_columns . clear ();

    for ( Cursors::iterator it = m_cursors . begin(); it != m_cursors . end(); ++it )
    {
        VCursorRelease ( *it );
    }

    for ( Databases::iterator it = m_databases . begin(); it != m_databases . end(); ++it )
    {
        VDatabaseRelease ( it -> second );
    }

    if ( m_schema != 0 )
    {
        VSchemaRelease ( m_schema );
        m_schema = 0;
    }

    if ( m_mgr != 0 )
    {
        VDBManagerRelease ( m_mgr );
        m_mgr = 0;
    }
}

const GeneralLoader :: DatabaseLoader :: Column*
GeneralLoader :: DatabaseLoader :: GetColumn ( uint32_t p_columnId ) const
{
    Columns::const_iterator curIt = m_columns . find ( p_columnId );
    if ( curIt != m_columns . end () )
    {
        return & curIt -> second;
    }
    else
    {
        return 0;
    }
}

rc_t
GeneralLoader :: DatabaseLoader :: UseSchema ( const string& p_file, const string& p_name )
{
    pLogMsg ( klogDebug, "database-loader: schema file '$(s1)', name '$(s2)'", "s1=%s,s2=%s",
                        p_file . c_str (), p_name . c_str () );

    rc_t rc = VDBManagerMakeUpdate ( & m_mgr, NULL );
    if ( rc == 0 )
    {
        for ( Paths::const_iterator it = m_includePaths . begin(); it != m_includePaths . end(); ++it )
        {
            rc = VDBManagerAddSchemaIncludePath ( m_mgr, "%s", it -> c_str() );
            if ( rc == 0 )
            {
                pLogMsg ( klogDebug,
                          "database-loader: Added schema include path '$(s)'",
                          "s=%s",
                          it -> c_str() );
            }
            else if ( GetRCObject ( rc ) == (RCObject)rcPath )
            {
                pLogMsg ( klogWarn,
                          "database-loader: Schema include path not found: '$(s)'",
                          "s=%s",
                          it -> c_str() );
                rc = 0;
            }
            else
            {
                return rc;
            }
        }

        rc = VDBManagerMakeSchema ( m_mgr, & m_schema );
        if ( rc  == 0 )
        {
            bool found = false;
            if ( ! p_file . empty () )
            {
                pLogMsg ( klogDebug,
                            "database-loader: VSchemaParseFile($(s))",
                            "s=%s",
                            p_file . c_str () );

                rc = VSchemaParseFile ( m_schema, "%s", p_file . c_str () );
                if ( rc == 0 )
                {
                    pLogMsg ( klogDebug,
                              "database-loader: Added schema file '$(s)'",
                              "s=%s",
                              p_file . c_str () );
                    found = true;
                }
                else if ( GetRCObject ( rc ) == (RCObject)rcPath && GetRCState ( rc ) == rcNotFound )
                {
                    pLogMsg ( klogWarn,
                              "database-loader: Schema file not found: '$(s)'",
                              "s=%s",
                              p_file . c_str () );
                    rc = 0;
                }
            }
            // if p_file is empty, there should be other schema files specified externally
            // through the command line options, in m_schemas

            if ( rc  == 0 )
            {
                for ( Paths::const_iterator it = m_schemas. begin(); it != m_schemas . end(); ++it )
                {
                    pLogMsg ( klogDebug,
                                "database-loader: cmdline VSchemaParseFile($(s))",
                                "s=%s",
                                it -> c_str() );
                    rc = VSchemaParseFile ( m_schema, "%s", it -> c_str() );
                    if ( rc == 0 )
                    {
                        pLogMsg ( klogDebug,
                                  "database-loader: Added schema file '$(s)'",
                                  "s=%s",
                                  it -> c_str() );
                        found = true;
                    }
                    else if ( GetRCObject ( rc ) == (RCObject)rcPath && GetRCState ( rc ) == rcNotFound )
                    {
                        pLogMsg ( klogWarn,
                                  "database-loader: Schema file not found: '$(s)'",
                                  "s=%s",
                                  it -> c_str() );
                        rc = 0;
                    }
                }
            }

            if ( found )
            {
                m_schemaName = p_name;
            }
            else
            {
                rc = RC ( rcVDB, rcMgr, rcCreating, rcSchema, rcNotFound );
            }
        }
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: RemotePath ( const string& p_path )
{
    if ( m_databaseNameOverridden )
    {
        pLogMsg ( klogWarn,
                  "database-loader: remote  path '$(s1)' ignored, overridden to '$(s2)'",
                  "s1=%s,s2=%s",
                  p_path . c_str (), m_databaseName . c_str () );
    }
    else
    {
        pLogMsg ( klogDebug, "database-loader: remote  path '$(s1)'", "s1=%s", p_path . c_str () );
        m_databaseName = p_path;
    }
    return 0;
}

static
void
check_vers_component ( const char * vers, const char * end, long num, unsigned long max, char term )
{
    if ( vers == end )
        throw "bad version";
    if ( * end != 0 && * end != term )
        throw "bad version";
    if ( num < 0 || (unsigned long)num > max )
        throw "bad version";
}

static
ver_t
string2ver_t ( const char * vers )
{
    ver_t ret = 0;
    char * end;
    long num = strtol ( vers, & end, 10 );
    check_vers_component ( vers, end, num, 255, '.' );
    if ( * end == '.' )
    {
        ret = num << 24;
        vers = end + 1;
        num = strtol ( vers, & end, 10 );
        check_vers_component ( vers, end, num, 255, '.' );
        if ( * end == '.' )
        {
            ret |= num << 16;
            vers = end + 1;
            num = strtol ( vers, & end, 10 );
            check_vers_component ( vers, end, num, 0xFFFF, 0 );
            ret |= num;
        }
    }
    return ret;
}

rc_t
GeneralLoader :: DatabaseLoader :: SoftwareName ( const string& p_name, const string& p_version )
{
    pLogMsg ( klogDebug, "database-loader: SoftwareName '$(n)', version '$(v)'", "n=%s,v=%s", p_name . c_str(), p_version . c_str() );
    try
    {
        m_softwareVersion = string2ver_t ( p_version.c_str() );
    }
    catch (...)
    {
        return RC ( rcExe, rcDatabase, rcCreating, rcMessage, rcBadVersion );
    }
    m_softwareName = p_name;
    return 0;
}

rc_t
GeneralLoader :: DatabaseLoader :: NewTable ( uint32_t p_tableId, const string& p_tableName )
{
    return AddMbrTbl ( p_tableId, 0, p_tableName, p_tableName, kcmCreate | kcmMD5 );
}

rc_t
GeneralLoader :: DatabaseLoader :: NewColumn ( uint32_t p_columnId, uint32_t p_tableId, uint32_t p_elemBits, uint8_t p_flagBits, const string& p_columnName )
{
    pLogMsg ( klogDebug, "database-loader: adding column '$(c)'", "c=%s", p_columnName . c_str() );

    rc_t rc = 0;
    Tables::const_iterator table = m_tables . find ( p_tableId );
    if ( table != m_tables . end() )
    {
        if ( m_columns . find ( p_columnId ) == m_columns . end () )
        {
            uint32_t cursor_idx = table -> second . cursorIdx;
            uint32_t column_idx;
            rc = VCursorAddColumn ( m_cursors [ cursor_idx ],
                                    & column_idx,
                                    "%s",
                                    p_columnName . c_str() );
            if ( rc == 0  )
            {
                Column col;
                col . name      = p_columnName;
                col . tableId   = p_tableId;
                col . cursorIdx = cursor_idx;
                col . columnIdx = column_idx;
                col . elemBits  = p_elemBits;
                col . flagBits  = p_flagBits;
                m_columns [ p_columnId ] = col;
                pLogMsg ( klogDebug,
                          "database-loader: tableId = $(t), added column '$(c)', columnIdx = $(i1), elemBits = $(i2), flagBits = $(i3)",
                          "t=%u,c=%s,i1=%u,i2=%u,i3=%u",
                          p_tableId, p_columnName . c_str(), col.columnIdx, col.elemBits, col.flagBits );
            }
        }
        else
        {
            rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcExists );
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcTable, rcInvalid );
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: AddMbrDB ( uint32_t p_objId, uint32_t p_parentId, const std :: string &p_mbrName, const std :: string &p_dbName, uint8_t p_createMode )
{
    pLogMsg ( klogDebug,
              "database-loader: adding database id=$(i) parent=$(p) mbrName='$(m)' dbName='$(n)' mode=$(d)",
              "m=%s,n=%s,i=%u,p=%u,d=%u",
              p_mbrName . c_str(), p_dbName . c_str (), p_objId, p_parentId, ( unsigned int ) p_createMode );

    rc_t rc = MakeDatabase ( p_parentId );
    if ( rc == 0 )
    {
        Databases :: const_iterator dad = m_databases . find ( p_parentId );
        assert ( dad != m_databases . end () );

        if ( m_databases . find ( p_objId ) == m_databases . end () )
        {
            VDatabase *db;
            rc = VDatabaseCreateDB ( dad -> second, & db, p_mbrName . c_str(), p_createMode, "%s", p_dbName . c_str () );
            if ( rc == 0 )
            {
                m_databases . insert ( Databases :: value_type ( p_objId, db ) );
                m_dbParents . insert ( DatabaseToParent :: value_type ( p_objId, p_parentId ) );
            }
        }
        else
        {
            rc = RC ( rcExe, rcFile, rcReading, rcDatabase, rcExists );
        }
    }

    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: AddMbrTbl ( uint32_t p_tblId, uint32_t p_dbId, const std :: string &p_mbrName, const std :: string &p_tblName, uint8_t p_createMode )
{
    pLogMsg ( klogDebug,
              "database-loader: adding table id=$(i) parent=$(p) mbrName='$(m)' dbName='$(n)' mode=$(d)",
              "m=%s,n=%s,i=%u,p=%u,d=%u",
              p_mbrName . c_str(), p_tblName . c_str (), p_tblId, p_dbId, ( unsigned int ) p_createMode );

    rc_t rc = 0;
    if ( m_tables . find ( p_tblId ) == m_tables . end() )
    {
        VTable* table;
        rc = MakeDatabase ( p_dbId );
        if ( rc == 0 )
        {
            Databases::iterator it = m_databases . find ( p_dbId );
            if ( it != m_databases . end() )
            {
                rc = VDatabaseCreateTable ( it -> second , & table, p_mbrName . c_str (), p_createMode, "%s", p_tblName . c_str ());
                if ( rc == 0 )
                {
                    VCursor* cursor;
                    rc = VTableCreateCursorWrite ( table, & cursor, kcmInsert );
                    if ( rc == 0 )
                    {
                        m_cursors . push_back ( cursor );
                        Table t;
                        t . name = p_tblName;
                        t . databaseId = p_dbId;
                        t . cursorIdx = ( uint32_t ) m_cursors . size() - 1;
                        m_tables [ p_tblId ] = t;
                    }
                    rc_t rc2 = VTableRelease ( table );
                    if ( rc == 0 )
                    {
                        rc = rc2;
                    }
                }
            }
            else
            {
                rc = RC ( rcExe, rcFile, rcReading, rcDatabase, rcNotFound );
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcTable, rcExists );
    }

    return rc;
}

static
rc_t WriteMetadata ( KMetadata* p_meta, const string& p_metadata_node, const string& p_value )
{
    KMDataNode* node;
    rc_t rc = KMetadataOpenNodeUpdate ( p_meta, & node, p_metadata_node . c_str () );
    if ( rc == 0 )
    {
        rc = KMDataNodeWrite ( node, p_value . c_str (), p_value . size () );

        rc_t rc2 = KMDataNodeRelease ( node );
        if ( rc == 0 )
        {
            rc = rc2;
        }
    }
    return rc;
}

static
rc_t WriteMetadataAttr ( KMetadata* p_meta, const string& p_metadata_node, const string& p_metadata_attr, const string& p_value )
{
    pLogMsg ( klogDebug,
              "database-loader: adding metadata attribute '$(a)=$(v)' to node $(n)",
              "a=%s,v=%s,n=%s",
              p_metadata_attr . c_str(), p_value.c_str(), p_metadata_node.c_str() );
    KMDataNode* node;
    rc_t rc = KMetadataOpenNodeUpdate ( p_meta, & node, p_metadata_node . c_str () );
    if ( rc == 0 )
    {
        rc = KMDataNodeWriteAttr ( node, p_metadata_attr . c_str (), p_value . c_str() );

        rc_t rc2 = KMDataNodeRelease ( node );
        if ( rc == 0 )
        {
            rc = rc2;
        }
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: DBMetadataNode ( uint32_t p_objId, const string& p_metadata_node, const string& p_value )
{
    pLogMsg ( klogDebug,
              "database-loader: adding metadata node '$(n)=$(v)' to database $(i)",
              "n=%s,v=%s,i=%u",
              p_metadata_node . c_str(), p_value.c_str(), p_objId );

    rc_t rc = 0;
    Databases::iterator it = m_databases . find ( p_objId );
    if ( it != m_databases . end() )
    {
        struct KMetadata* meta;
        rc = VDatabaseOpenMetadataUpdate ( it -> second, & meta );
        if ( rc == 0 )
        {
            rc = WriteMetadata ( meta,p_metadata_node, p_value );
            rc_t rc2 = KMetadataRelease ( meta );
            if ( rc == 0 )
            {
                rc = rc2;
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcDatabase, rcNotFound );
    }

    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: DBMetadataNodeAttr ( uint32_t p_objId, const string& p_metadata_node, const string& p_attr, const string& p_value )
{
    pLogMsg ( klogDebug,
              "database-loader: setting metadata node attr '$(n) $(a)=$(v)' to database $(i)",
              "n=%s,a=%s,v=%s,i=%u",
              p_metadata_node . c_str(), p_attr . c_str(), p_value.c_str(), p_objId );

    rc_t rc = 0;
    Databases::iterator it = m_databases . find ( p_objId );
    if ( it != m_databases . end() )
    {
        struct KMetadata* meta;
        rc = VDatabaseOpenMetadataUpdate ( it -> second, & meta );
        if ( rc == 0 )
        {
            rc = WriteMetadataAttr ( meta, p_metadata_node, p_attr, p_value );
            rc_t rc2 = KMetadataRelease ( meta );
            if ( rc == 0 )
            {
                rc = rc2;
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcDatabase, rcNotFound );
    }

    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: TblMetadataNode ( uint32_t p_objId, const string& p_metadata_node, const string& p_value )
{
    pLogMsg ( klogDebug,
              "database-loader: adding metadata node '$(n)=$(v)' to table $(i)",
              "n=%s,v=%s,i=%u",
              p_metadata_node . c_str(), p_value.c_str(), p_objId );

    rc_t rc = 0;
    Tables::iterator it = m_tables . find ( p_objId );
    if ( it != m_tables . end() )
    {
        struct VTable* tbl;
        assert ( m_cursors [ it -> second . cursorIdx ] );
        rc = VCursorOpenParentUpdate ( m_cursors [ it -> second . cursorIdx ], &tbl );
        if ( rc == 0 )
        {
            struct KMetadata* meta;
            rc = VTableOpenMetadataUpdate ( tbl, & meta );
            if ( rc == 0 )
            {
                rc = WriteMetadata ( meta,p_metadata_node, p_value );
                rc_t rc2 = KMetadataRelease ( meta );
                if ( rc == 0 )
                {
                    rc = rc2;
                }
            }
            rc_t rc2 = VTableRelease ( tbl );
            if ( rc == 0 )
            {
                rc = rc2;
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcTable, rcNotFound );
    }

    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: TblMetadataNodeAttr ( uint32_t p_objId, const string& p_metadata_node, const string& p_attr, const string& p_value )
{
    pLogMsg ( klogDebug,
              "database-loader: adding metadata node '$(n)=$(v)' to table $(i)",
              "n=%s,v=%s,i=%u",
              p_metadata_node . c_str(), p_value.c_str(), p_objId );

    rc_t rc = 0;
    Tables::iterator it = m_tables . find ( p_objId );
    if ( it != m_tables . end() )
    {
        struct VTable* tbl;
        assert ( m_cursors [ it -> second . cursorIdx ] );
        rc = VCursorOpenParentUpdate ( m_cursors [ it -> second . cursorIdx ], &tbl );
        if ( rc == 0 )
        {
            struct KMetadata* meta;
            rc = VTableOpenMetadataUpdate ( tbl, & meta );
            if ( rc == 0 )
            {
                rc = WriteMetadataAttr ( meta, p_metadata_node, p_attr, p_value );
                rc_t rc2 = KMetadataRelease ( meta );
                if ( rc == 0 )
                {
                    rc = rc2;
                }
            }
            rc_t rc2 = VTableRelease ( tbl );
            if ( rc == 0 )
            {
                rc = rc2;
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcTable, rcNotFound );
    }

    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: ColMetadataNode ( uint32_t p_objId, const string& p_metadata_node, const string& p_value )
{
    pLogMsg ( klogDebug,
              "database-loader: adding metadata node '$(n)=$(v)' to column $(i)",
              "n=%s,v=%s,i=%u",
              p_metadata_node . c_str(), p_value.c_str(), p_objId );

    rc_t rc = 0;
    Columns::iterator it = m_columns . find ( p_objId );
    if ( it != m_columns . end() )
    {
        it -> second . metadata [ p_metadata_node ] = p_value;
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
    }

    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: ColMetadataNodeAttr ( uint32_t p_objId, const string& p_metadata_node, const string& p_attr, const string& p_value )
{
    pLogMsg ( klogDebug,
              "database-loader: adding metadata attribute '$(a)=$(v)' to node $(n), column $(i)",
              "a=%s,v=%s,n=%s,i=%u",
              p_attr . c_str(), p_value.c_str(), p_metadata_node.c_str(), p_objId );

    rc_t rc = 0;
    Columns::iterator it = m_columns . find ( p_objId );
    if ( it != m_columns . end() )
    {
        it -> second . metadata_attributes [ p_metadata_node ] [ p_attr ]= p_value;
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
    }

    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: SaveColumnMetadata ( const Column& p_col )
{
    if ( p_col . metadata . size () == 0 && p_col . metadata_attributes . size() == 0 )
    {
        return 0;
    }

    assert ( m_tables . find ( p_col . tableId ) != m_tables.end() );
    const Table& t = m_tables [ p_col . tableId ];

    assert ( m_databases . find ( t . databaseId ) != m_databases.end() );
    assert ( m_databases . find ( t . databaseId ) -> second != 0 );

    VTable* table;
    rc_t rc = VDatabaseOpenTableUpdate ( m_databases [ t . databaseId ], & table, t . name . c_str () );
    if ( rc == 0 )
    {
        KTable* ktbl;
        rc = VTableOpenKTableUpdate ( table, & ktbl );
        if ( rc == 0 )
        {
            KColumn* col;
            rc = KTableOpenColumnUpdate ( ktbl, & col, p_col . name . c_str () );
            if ( rc == 0 )
            {
                KMetadata *meta;
                rc = KColumnOpenMetadataUpdate ( col, &meta );
                if ( rc == 0 )
                {
                    for ( Column :: Metadata :: const_iterator it = p_col . metadata . begin(); it != p_col . metadata . end(); ++ it )
                    {
                        rc = WriteMetadata ( meta, it -> first . c_str (), it -> second . c_str () );
                        if ( rc != 0 )
                        {
                            break;
                        }
                    }
                    if ( rc == 0 )
                    {   // metadata node attributes
                        for ( auto it : p_col . metadata_attributes )
                        {
                            for ( auto sub_it : it . second )
                            {
                                rc = WriteMetadataAttr ( meta, it . first . c_str(), sub_it . first . c_str (), sub_it . second . c_str () );
                                if ( rc != 0 )
                                {
                                    break;
                                }
                            }
                        }
                    }

                    rc_t rc2 = KMetadataRelease ( meta );
                    if ( rc == 0 )
                    {
                        rc = rc2;
                    }
                }
                rc_t rc2 = KColumnRelease ( col );
                if ( rc == 0 )
                {
                    rc = rc2;
                }
            }
            rc_t rc2 = KTableRelease ( ktbl );
            if ( rc == 0 )
            {
                rc = rc2;
            }
        }
        rc_t rc2 = VTableRelease ( table );
        if ( rc == 0 )
        {
            rc = rc2;
        }
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: CursorWrite ( const struct Column& p_col, const void* p_data, size_t p_size )
{
    return VCursorWrite ( m_cursors [ p_col . cursorIdx ],
                          p_col . columnIdx,
                          p_col . elemBits,
                          p_data,
                          0,
                          p_size );
}

rc_t
GeneralLoader :: DatabaseLoader :: CursorDefault ( const struct Column& p_col, const void* p_data, size_t p_size )
{
    return VCursorDefault ( m_cursors [ p_col . cursorIdx ],
                            p_col . columnIdx,
                            p_col . elemBits,
                            p_data,
                            0,
                            p_size );
}

rc_t
GeneralLoader :: DatabaseLoader :: CellData ( uint32_t p_columnId, const void* p_data, size_t p_elemCount )
{
    rc_t rc = 0;
    Columns::const_iterator curIt = m_columns . find ( p_columnId );
    if ( curIt != m_columns . end () )
    {
        const Column& col = curIt -> second;
        pLogMsg ( klogDebug,
                  "database-loader: columnIdx = $(i), elem size=$(s) bits, elem count=$(c)",
                  "i=%u,s=%u,c=%u",
                  col . columnIdx, col . elemBits, p_elemCount );
        rc = CursorWrite ( col, p_data, p_elemCount );
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: CellDefault ( uint32_t p_columnId, const void* p_data, size_t p_elemCount )
{   //TODO: this and Handle_CellData are almost identical - refactor
    rc_t rc = 0;
    Columns::const_iterator curIt = m_columns . find ( p_columnId );
    if ( curIt != m_columns . end () )
    {
        const Column& col = curIt -> second;
        pLogMsg ( klogDebug,
                  "database-loader: columnIdx = $(i), elem size=$(s) bits, elem count=$(c)",
                  "i=%u,s=%u,c=%u",
                  col . columnIdx, col . elemBits, p_elemCount );
        rc = CursorDefault ( col, p_data, p_elemCount );
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: MakeDatabase( uint32_t p_id )
{
    rc_t rc = 0;

    Databases::iterator it = m_databases . find ( p_id );
    if ( it != m_databases . end() )
    {
        VDatabase*& db = it -> second;
        if ( db == 0 ) // only create once
        {
            rc = VDBManagerCreateDB ( m_mgr,
                                      & db,
                                      m_schema,
                                      m_schemaName . c_str (),
                                      kcmInit + kcmMD5,
                                      "%s",
                                      m_databaseName . c_str () );
            if ( rc == 0 && p_id == 0 )
            {   // populate the root database's metadata
                struct KMetadata* meta;
                rc = VDatabaseOpenMetadataUpdate ( db, &meta );
                if ( rc == 0 )
                {
                    KMDataNode *node;
                    rc = KMetadataOpenNodeUpdate ( meta, &node, "/" );

                    if (rc == 0)
                    {
                        rc = KLoaderMeta_WriteWithVersion ( node, m_programName.c_str(), __DATE__, GetKAppVersion(), m_softwareName.c_str(), m_softwareVersion );
                        KMDataNodeRelease(node);
                    }

                    rc_t rc2 = KMetadataRelease ( meta );
                    if ( rc == 0 )
                    {
                        rc = rc2;
                    }
                }
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcDatabase, rcNotFound );
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: OpenStream ()
{
    pLogMsg ( klogDebug,
              "database-loader: Database created, schema spec='$(s)', database='$(d)'",
              "s=%s,d=%s",
              m_schemaName . c_str (), m_databaseName . c_str () );

    rc_t rc = MakeDatabase ( 0 );
    if ( rc == 0 )
    {
        for ( Cursors::iterator it = m_cursors . begin(); it != m_cursors . end(); ++it )
        {
            rc_t rc = VCursorOpen ( *it  );
            if ( rc != 0 )
            {
                return rc;
            }
            rc = VCursorOpenRow ( *it );
            if ( rc != 0 )
            {
                break;
            }
        }
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: CloseStream ()
{
    rc_t rc = 0;
    rc_t rc2 = 0;

    for ( Cursors::iterator it = m_cursors . begin(); it != m_cursors . end(); ++it )
    {
        rc = VCursorCloseRow ( *it );
        if ( rc == 0 )
        {
            rc = VCursorCommit ( *it );
            if ( rc == 0 )
            {
                struct VTable* table;
                rc = VCursorOpenParentUpdate ( *it, &table );
                if ( rc == 0 )
                {
                    rc = VCursorRelease ( *it );
                    if ( rc == 0 )
                    {
                        rc = VTableReindex ( table );
                    }
                }
                rc2 = VTableRelease ( table );
                if ( rc == 0 )
                {
                    rc = rc2;
                }
            }
        }
        if ( rc != 0 )
        {
            break;
        }
    }
    m_cursors . clear ();

    if ( rc == 0 )
    {   // save column-level metadata collected from ColMetadata events
        for ( Columns::iterator it = m_columns. begin(); it != m_columns. end(); ++it )
        {
            rc = SaveColumnMetadata ( it -> second );
            if ( rc != 0 )
            {
                break;
            }
        }
    }

    for ( Databases::iterator it = m_databases . begin(); it != m_databases . end(); ++it )
    {
        VDatabaseRelease ( it -> second );
    }
    m_databases . clear();

    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: NextRow ( uint32_t p_tableId )
{
    rc_t rc = 0;
    Tables::const_iterator table = m_tables . find ( p_tableId );
    if ( table != m_tables . end() )
    {
        VCursor * cursor = m_cursors [ table -> second . cursorIdx ];
        rc = VCursorCommitRow ( cursor );
        if ( rc == 0 )
        {
            rc = VCursorCloseRow ( cursor );
            if ( rc == 0 )
            {
                rc = VCursorOpenRow ( cursor );
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcTable, rcNotFound );
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: MoveAhead ( uint32_t p_tableId, uint64_t p_count )
{
    rc_t rc = 0;
    Tables::const_iterator table = m_tables . find ( p_tableId );
    if ( table != m_tables . end() )
    {
        VCursor * cursor = m_cursors [ table -> second . cursorIdx ];
        for ( uint64_t i = 0; i < p_count; ++i )
        {   // for now, simulate proper handling (this will commit the current row and insert count-1 empty rows)
            rc = VCursorCommitRow ( cursor );
            if ( rc != 0 )
            {
                break;
            }
            rc = VCursorCloseRow ( cursor );
            if ( rc != 0 )
            {
                break;
            }
            rc = VCursorOpenRow ( cursor );
            if ( rc != 0 )
            {
                break;
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcTable, rcNotFound );
    }
    return rc;
}

rc_t
GeneralLoader :: DatabaseLoader :: ErrorMessage ( const string & p_text )
{
    pLogMsg ( klogErr, "general-loader: error \"$(t)\"", "t=%s", p_text . c_str () );
    return RC ( rcExe, rcFile, rcReading, rcError, rcExists );
}

rc_t
GeneralLoader :: DatabaseLoader :: LogMessage ( const string & p_text )
{
    pLogMsg ( klogInfo, "general-loader: log from $(s): \"$(t)\"", "s=%s,t=%s", m_softwareName . c_str (), p_text . c_str () );
    return 0;
}


/*
<Log>
  <status app="sra-stat" message="processed 12%" pid="60234"
        timestamp="2015-09-21T19:49:45" version="2.5.1" percent="12"/>
</Log>
 */
rc_t
GeneralLoader :: DatabaseLoader :: ProgressMessage ( const std :: string& p_name, uint32_t p_pid,
                                                     uint32_t p_timestamp, uint32_t p_version, uint32_t p_percent )
{
    KTime kt;
    KTimeLocal ( &kt, ( KTime_t ) p_timestamp );

    pLogMsg ( klogInfo,
              "processed $(percent)%"
              ,
              "app=%s,pid=%u,timestamp=%lT,version=%V,percent=%u"
              ,
              p_name . c_str (),
              p_pid,
              & kt,
              ( ver_t ) p_version,
              p_percent );
    return 0;
}
