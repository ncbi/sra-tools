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

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/cursor.h>
#include <vdb/table.h>

#include <algorithm>

using namespace std;

///////////// GeneralLoader::DatabaseLoader

GeneralLoader :: DatabaseLoader :: DatabaseLoader ( const Paths& p_includePaths, const Paths& p_schemas, const std::string& p_dbNameOverride )
:   m_includePaths ( p_includePaths ),
    m_schemas ( p_schemas ),
    m_databaseName ( p_dbNameOverride ), // if specified, overrides the database path coming in from the stream
    m_mgr ( 0 ),
    m_schema ( 0 ),
    m_db ( 0 ),
    m_databaseNameOverridden ( ! m_databaseName.empty() ) 
{
}

GeneralLoader :: DatabaseLoader :: ~DatabaseLoader ()
{
    m_tables . clear();
    m_columns . clear ();
    
    for ( Cursors::iterator it = m_cursors . begin(); it != m_cursors . end(); ++it )
    {
        VCursorRelease ( *it );
    }
    m_cursors . clear();

    if ( m_db != 0 )
    {
        VDatabaseRelease ( m_db );
        m_db = 0;
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
    pLogMsg ( klogInfo, "database-loader: schema file '$(s1)', name '$(s2)'", "s1=%s,s2=%s", 
                        p_file . c_str (), p_name . c_str () );

    rc_t rc = VDBManagerMakeUpdate ( & m_mgr, NULL );
    if ( rc == 0 )
    {
        for ( Paths::const_iterator it = m_includePaths . begin(); it != m_includePaths . end(); ++it )
        {   
            rc = VDBManagerAddSchemaIncludePath ( m_mgr, "%s", it -> c_str() );
            if ( rc == 0 )
            {
                pLogMsg ( klogInfo, 
                          "database-loader: Added schema include path '$(s)'", 
                          "s=%s", 
                          it -> c_str() );
            }
            else if ( GetRCObject ( rc ) == (RCObject)rcPath )
            {
                pLogMsg ( klogInfo, 
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
                rc = VSchemaParseFile ( m_schema, "%s", p_file . c_str () );
                if ( rc == 0 )
                {
                    pLogMsg ( klogInfo, 
                              "database-loader: Added schema file '$(s)'", 
                              "s=%s", 
                              p_file . c_str () );
                    found = true;
                }
                else if ( GetRCObject ( rc ) == (RCObject)rcPath && GetRCState ( rc ) == rcNotFound )
                {
                    pLogMsg ( klogInfo, 
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
                    rc = VSchemaParseFile ( m_schema, "%s", it -> c_str() );
                    if ( rc == 0 )
                    {
                        pLogMsg ( klogInfo, 
                                  "database-loader: Added schema file '$(s)'", 
                                  "s=%s", 
                                  it -> c_str() );
                        found = true;
                    }
                    else if ( GetRCObject ( rc ) == (RCObject)rcPath && GetRCState ( rc ) == rcNotFound )
                    {
                        pLogMsg ( klogInfo, 
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
        pLogMsg ( klogInfo, 
                  "database-loader: remote  path '$(s1)' ignored, overridden to '$(s2)'", 
                  "s1=%s,s2=%s", 
                  p_path . c_str (), m_databaseName . c_str () );
    }
    else
    {
        pLogMsg ( klogInfo, "database-loader: remote  path '$(s1)'", "s1=%s", p_path . c_str () );
        m_databaseName = p_path;
    }
    return 0;
}

rc_t 
GeneralLoader :: DatabaseLoader :: NewTable ( uint32_t p_tableId, const string& p_tableName )
{   
    rc_t rc = 0;
    if ( m_tables . find ( p_tableId ) == m_tables . end() )
    {
        VTable* table;
        rc = MakeDatabase();
        if ( rc == 0 )
        {
            rc = VDatabaseCreateTable ( m_db, & table, p_tableName . c_str (), kcmCreate | kcmMD5, "%s", p_tableName . c_str ());
            if ( rc == 0 )
            {
                VCursor* cursor;
                rc = VTableCreateCursorWrite ( table, & cursor, kcmInsert );
                if ( rc == 0 )
                {
                    m_cursors . push_back ( cursor );
                    m_tables [ p_tableId ] = ( uint32_t ) m_cursors . size() - 1;
                }
                rc_t rc2 = VTableRelease ( table );
                if ( rc == 0 )
                {
                    rc = rc2;
                }
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcTable, rcExists );
    }

    return rc;
}

rc_t 
GeneralLoader :: DatabaseLoader :: NewColumn ( uint32_t p_columnId, uint32_t p_tableId, uint32_t p_elemBits, uint8_t p_flagBits, const string& p_columnName )
{
    pLogMsg ( klogInfo, "database-loader: adding column '$(c)'", "c=%s", p_columnName . c_str() );
    
    rc_t rc = 0;
    TableIdToCursor::const_iterator table = m_tables . find ( p_tableId );
    if ( table != m_tables . end() )
    {
        if ( m_columns . find ( p_columnId ) == m_columns . end () )
        {
            uint32_t cursor_idx = table -> second;
            uint32_t column_idx;
            rc = VCursorAddColumn ( m_cursors [ cursor_idx ], 
                                    & column_idx, 
                                    "%s", 
                                    p_columnName . c_str() );
            if ( rc == 0  )
            {
                Column col;
                col . cursorIdx = cursor_idx;
                col . columnIdx = column_idx;
                col . elemBits  = p_elemBits;
                col . flagBits  = p_flagBits;
                m_columns [ p_columnId ] = col;
                pLogMsg ( klogInfo, 
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
        pLogMsg ( klogInfo,     
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
        pLogMsg ( klogInfo,     
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
GeneralLoader :: DatabaseLoader :: MakeDatabase()
{
    if ( m_db == 0 )
    {
        return VDBManagerCreateDB ( m_mgr, 
                                    & m_db, 
                                    m_schema, 
                                    m_schemaName . c_str (), 
                                    kcmInit + kcmMD5, 
                                    "%s", 
                                    m_databaseName . c_str () );
    }
    return 0;
}

rc_t 
GeneralLoader :: DatabaseLoader :: OpenStream ()
{
    pLogMsg ( klogInfo, 
              "database-loader: Database created, schema spec='$(s)', database='$(d)'", 
              "s=%s,d=%s", 
              m_schemaName . c_str (), m_databaseName . c_str () );
              
    rc_t rc = MakeDatabase ();
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

    rc2 = VDatabaseRelease ( m_db );
    if ( rc == 0 )
    {
        rc = rc2;
    }
    m_db = 0;

    return rc;
}

rc_t 
GeneralLoader :: DatabaseLoader :: NextRow ( uint32_t p_tableId )
{
    rc_t rc = 0;
    TableIdToCursor::const_iterator table = m_tables . find ( p_tableId );
    if ( table != m_tables . end() )
    {
        VCursor * cursor = m_cursors [ table -> second ];
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
    TableIdToCursor::const_iterator table = m_tables . find ( p_tableId );
    if ( table != m_tables . end() )
    {
        VCursor * cursor = m_cursors [ table -> second ];
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
