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

#include <kns/stream.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/cursor.h>
#include <vdb/table.h>

#include <cstring>

using namespace std;

///////////// GeneralLoader::Reader

GeneralLoader::Reader::Reader( const struct KStream& p_input )
:   m_input ( p_input ),
    m_buffer ( 0 ),
    m_bufSize ( 0 ),
    m_readCount ( 0 )
{
    KStreamAddRef ( & m_input );
}

GeneralLoader::Reader::~Reader()
{
    KStreamRelease ( & m_input );
    free ( m_buffer );
}

rc_t 
GeneralLoader::Reader::Read( void * p_buffer, size_t p_size )
{
    pLogMsg ( klogInfo, "general-loader: reading $(s) bytes", "s=%u", ( unsigned int ) p_size );

    m_readCount += p_size;
    return KStreamReadExactly ( & m_input, p_buffer, p_size );
}

rc_t 
GeneralLoader::Reader::Read( size_t p_size )
{
    if ( p_size > m_bufSize )
    {
        m_buffer = realloc ( m_buffer, p_size );
        if ( m_buffer == 0 )
        {
            m_bufSize = 0;
            m_readCount = 0;
            return RC ( rcExe, rcFile, rcReading, rcMemory, rcExhausted );
        }
    }
    
    pLogMsg ( klogInfo, "general-loader: reading $(s) bytes", "s=%u", ( unsigned int ) p_size );
    
    m_readCount += p_size;
    return KStreamReadExactly ( & m_input, m_buffer, p_size );
}

void 
GeneralLoader::Reader::Align( uint8_t p_bytes )
{
    if ( m_readCount % p_bytes != 0 )
    {
        Read ( p_bytes - m_readCount % p_bytes );
    }
}


///////////// GeneralLoader

GeneralLoader::GeneralLoader ( const struct KStream& p_input )
:   m_reader ( p_input ),
    m_mgr ( 0 ),
    m_schema ( 0 ),
    m_db ( 0 )
{
}

GeneralLoader::~GeneralLoader ()
{
    Reset();
}

void
GeneralLoader::SplitAndAdd( Paths& p_paths, const string& p_path )
{
    size_t startPos = 0;
    size_t colonPos = p_path . find ( ':', startPos );
    while ( colonPos != string::npos )
    {
        p_paths . push_back ( p_path . substr ( startPos, colonPos - startPos ) );
        startPos = colonPos + 1;
        colonPos = p_path . find ( ':', startPos );    
    }
    p_paths . push_back ( p_path . substr ( startPos ) );
}

void 
GeneralLoader::AddSchemaIncludePath( const string& p_path )
{
    SplitAndAdd ( m_includePaths, p_path );
}

void 
GeneralLoader::AddSchemaFile( const string& p_path )
{
    SplitAndAdd ( m_schemas, p_path );
}

void
GeneralLoader::Reset()
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

void 
GeneralLoader::CleanUp()
{   // remove the database
    if ( ! m_databaseName . empty () )
    {
        Reset();
        
        KDirectory* wd;
        KDirectoryNativeDir ( & wd );
        KDirectoryRemove ( wd, true, m_databaseName. c_str () );
        KDirectoryRelease ( wd );
    }
}

rc_t 
GeneralLoader::Run()
{
    Reset();
    
    rc_t rc = ReadHeader ();
    if ( rc == 0 ) 
    {
        rc = m_header . packing ? ReadPackedEvents () : ReadUnpackedEvents ();
    }
    
    if ( rc != 0 )
    {
        CleanUp();
    }
    
    return rc;
}

rc_t 
GeneralLoader::ReadHeader ()
{
    rc_t rc = m_reader . Read ( & m_header, sizeof m_header );
    if ( rc == 0 )
    { 
        if ( strncmp ( m_header . dad . signature, GeneralLoaderSignatureString, sizeof ( m_header . dad . signature ) ) != 0 )
        {
            rc = RC ( rcExe, rcFile, rcReading, rcHeader, rcCorrupt );
        }
        else 
        {
            switch ( m_header . dad . endian )
            {
            case GW_GOOD_ENDIAN:
                if ( m_header . dad . version != GW_CURRENT_VERSION )
                {
                    rc = RC ( rcExe, rcFile, rcReading, rcHeader, rcBadVersion );
                }
                else
                {
                    rc = 0;
                }
                break;
            case GW_REVERSE_ENDIAN:
                LogMsg ( klogInfo, "general-loader event: Detected reverse endianness (not yet supported)" );
                rc = RC ( rcExe, rcFile, rcReading, rcFormat, rcUnsupported );
                //TODO: apply byte order correction before checking the version number
                break;
            default:
                rc = RC ( rcExe, rcFile, rcReading, rcFormat, rcInvalid );
                break;
            }
        }
    }
    
    if ( rc == 0 && m_header . dad . hdr_size > sizeof m_header ) 
    {   
        rc = m_reader . Read ( m_header . dad . hdr_size - sizeof m_header );
    }
    return rc;
}

rc_t
GeneralLoader :: ReadUnpackedEvents()
{
    rc_t rc;
    do 
    { 
        m_reader . Align ();
        
        struct gw_evt_hdr_v1 evt_header;
        rc = m_reader . Read ( & evt_header, sizeof ( evt_header ) );    
        if ( rc != 0 )
        {
            break;
        }
        
        switch ( ncbi :: evt ( evt_header ) )
        {
        case evt_use_schema:
            {
                LogMsg ( klogInfo, "general-loader event: Use-Schema" );
            
                uint32_t schema_file_size;
                rc = m_reader . Read ( & schema_file_size, sizeof ( schema_file_size ) );
                if ( rc == 0 )
                {
                    uint32_t schema_name_size;
                    rc = m_reader . Read ( & schema_name_size, sizeof ( schema_name_size ) );
                    if ( rc == 0 )
                    {
                        rc = m_reader . Read ( schema_file_size + schema_name_size );
                        if ( rc == 0 )
                        {
                            rc = Handle_UseSchema ( string ( ( const char * ) m_reader . GetBuffer (), schema_file_size ), 
                                                    string ( ( const char * ) m_reader . GetBuffer () + schema_file_size, schema_name_size ) );
                        }
                    }
                }
            }
            break;

        case evt_remote_path:
            {
                LogMsg ( klogInfo, "general-loader event: Remote-Path" );
                
                uint32_t database_name_size;
                rc = m_reader . Read ( & database_name_size, sizeof ( database_name_size ) );
                if ( rc == 0 )
                {
                    rc = m_reader . Read ( database_name_size );
                    if ( rc == 0 )
                    {
                        rc = Handle_RemotePath ( string ( ( const char * ) m_reader . GetBuffer (), database_name_size ) );
                    }
                }
            }
            break;
            
        case evt_new_table:
            {
                uint32_t tableId = ncbi :: id ( evt_header );
                pLogMsg ( klogInfo, "general-loader event: New-Table, id=$(i)", "i=%u", tableId );
                
                if ( m_tables . find ( tableId ) == m_tables . end() )
                {
                    uint32_t table_name_size;
                    rc = m_reader . Read ( & table_name_size, sizeof ( table_name_size ) );
                    if ( rc == 0 )
                    {
                        rc = m_reader . Read ( table_name_size );
                        if ( rc == 0 )
                        {
                            rc = Handle_NewTable ( tableId, string ( ( const char * ) m_reader . GetBuffer (), table_name_size ) );
                        }
                    }
                }
                else
                {
                    rc = RC ( rcExe, rcFile, rcReading, rcTable, rcExists );
                }
            }  
            break;
            
        case evt_new_column:
            {
                uint32_t columnId = ncbi :: id ( evt_header );
                pLogMsg ( klogInfo, "general-loader event: New-Column, id=$(i)", "i=%u", columnId );
    
                uint32_t table_id;
                rc = m_reader . Read ( & table_id , sizeof ( table_id ) );
                if ( rc == 0 )
                {
                    uint32_t elem_size;
                    rc = m_reader . Read ( & elem_size , sizeof ( elem_size ) );
                    if ( rc == 0 )
                    {
                        uint32_t col_name_size;
                        rc = m_reader . Read ( & col_name_size, sizeof ( col_name_size ) );
                        if ( rc == 0 )
                        {
                            rc = m_reader . Read ( col_name_size );
                            if ( rc == 0 )
                            {
                                rc = Handle_NewColumn ( columnId, 
                                                        table_id, 
                                                        elem_size, 
                                                        0,
                                                        string ( ( const char * ) m_reader . GetBuffer (), col_name_size ) );
                            }
                        }
                    }
                }
            }
            break;

        case evt_cell_data:
            rc = Handle_CellData ( ncbi :: id ( evt_header ) );
            break;
            
        case evt_cell_default: 
            rc = Handle_CellDefault ( ncbi :: id ( evt_header ) );
            break;
            
        case evt_open_stream:
            LogMsg ( klogInfo, "general-loader event: Open-Stream" );
            rc = OpenCursors ();
            break; 
            
        case evt_end_stream:
            LogMsg ( klogInfo, "general-loader event: End-Stream" );
            return CloseCursors ();
            
        case evt_next_row:
            rc = HandleNextRow ( ncbi :: id ( evt_header ) );
            break;
            
        case evt_move_ahead:
            {
                uint32_t table_id = ncbi :: id ( evt_header );
                uint64_t count;
                rc = m_reader . Read ( & count, sizeof ( count ) );  
                if ( rc == 0 )
                {
                    rc = Handle_MoveAhead ( table_id, count );
                }
            }
            break;
            
        case evt_errmsg:
            LogMsg ( klogInfo, "general-loader event: Error-Message" );
            {   
                uint32_t message_size;
                rc = m_reader . Read ( & message_size, sizeof ( message_size ) );
                if ( rc == 0 )
                {
                    rc = m_reader . Read ( message_size );
                    if ( rc == 0 )
                    {
                        rc = Handle_ErrorMessage ( string ( ( const char * ) m_reader . GetBuffer (), message_size ) );
                    }
                }
            }
            break;
            
        default:
            pLogMsg ( klogErr, "unexpected general-loader event: $(e)", "e=%i", ( int ) ncbi :: evt ( evt_header ) );
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        }
    }
    while ( rc == 0 );
    
    return rc;
}

rc_t 
GeneralLoader :: ReadPackedEvents()
{
    rc_t rc;
    do 
    { 
        m_reader . Align ();
        
        struct gwp_evt_hdr_v1 evt_header;
        rc = m_reader . Read ( & evt_header, sizeof ( evt_header ) );    
        if ( rc != 0 )
        {
            break;
        }
        
        switch ( ncbi :: evt ( evt_header ) )
        {
        case evt_use_schema2:
            {
                LogMsg ( klogInfo, "general-loader event: Use-Schema2" );
            
                uint16_t schema_file_size;
                rc = m_reader . Read ( & schema_file_size, sizeof ( schema_file_size ) );
                if ( rc == 0 )
                {
                    uint16_t schema_name_size;
                    rc = m_reader . Read ( & schema_name_size, sizeof ( schema_name_size ) );
                    if ( rc == 0 )
                    {
                        // restore the actual string size
                        ++schema_file_size;
                        ++schema_name_size;
                        
                        rc = m_reader . Read ( schema_file_size + schema_name_size );
                        if ( rc == 0 )
                        {
                            rc = Handle_UseSchema ( string ( ( const char * ) m_reader . GetBuffer (), schema_file_size ), 
                                                    string ( ( const char * ) m_reader . GetBuffer () + schema_file_size, schema_name_size ) );
                        }
                    }
                }
            }
            break;
            
        case evt_remote_path2:
            {
                LogMsg ( klogInfo, "general-loader event: Remote-Path2" );
                
                uint16_t database_name_size;
                rc = m_reader . Read ( & database_name_size, sizeof ( database_name_size ) );
                if ( rc == 0 )
                {
                    // restore the actual string size
                    ++database_name_size;
                    
                    rc = m_reader . Read ( database_name_size );
                    if ( rc == 0 )
                    {
                        rc = Handle_RemotePath(string ( ( const char * ) m_reader . GetBuffer (), database_name_size ));
                    }
                }
            }
            break;
            
        case evt_new_table2:
            {
                uint32_t tableId = ncbi :: id ( evt_header );
                pLogMsg ( klogInfo, "general-loader event: New-Table2, id=$(i)", "i=%u", tableId );
                
                if ( m_tables . find ( tableId ) == m_tables . end() )
                {
                    uint16_t table_name_size;
                    rc = m_reader . Read ( & table_name_size, sizeof ( table_name_size ) );
                    if ( rc == 0 )
                    {
                        ++table_name_size;
                        
                        rc = m_reader . Read ( table_name_size );
                        if ( rc == 0 )
                        {
                            rc = Handle_NewTable ( tableId, string ( ( const char * ) m_reader . GetBuffer (), table_name_size ) );
                        }
                    }
                }
                else
                {
                    rc = RC ( rcExe, rcFile, rcReading, rcTable, rcExists );
                }
            }  
            break;
            
        case evt_new_column:
            {
                uint32_t columnId = ncbi :: id ( evt_header );
                pLogMsg ( klogInfo, "general-loader event: New-Column, id=$(i)", "i=%u", columnId );
                
                uint8_t table_id;
                rc = m_reader . Read ( & table_id , sizeof ( table_id ) );
                if ( rc == 0 )
                {
                    uint8_t elem_size;
                    rc = m_reader . Read ( & elem_size , sizeof ( elem_size ) );
                    if ( rc == 0 )
                    {
                        uint8_t flag_bits;
                        rc = m_reader . Read ( & flag_bits , sizeof ( flag_bits ) );
                        if ( rc == 0 )
                        {
                            uint8_t col_name_size;
                            rc = m_reader . Read ( & col_name_size, sizeof ( col_name_size ) );
                            if ( rc == 0 )
                            {
                                rc = m_reader . Read ( col_name_size + 1 );
                                if ( rc == 0 )
                                {
                                    rc = Handle_NewColumn ( columnId, 
                                                            ( uint32_t ) table_id + 1, 
                                                            ( uint32_t ) elem_size + 1, 
                                                            flag_bits,
                                                            string ( ( const char * ) m_reader . GetBuffer (), ( uint32_t ) col_name_size + 1 ) );
                                }
                            }
                        }
                    }
                }
            }
            break;

        case evt_open_stream:
            LogMsg ( klogInfo, "general-loader event: Open-Stream" );
            rc = OpenCursors ();
            break; 
            
        case evt_end_stream:
            LogMsg ( klogInfo, "general-loader event: End-Stream" );
            return CloseCursors ();
            
        case evt_cell_data2:
            rc = Handle_CellData_Packed ( ncbi :: id ( evt_header ) );
            break;
            
        case evt_cell_default2:
            rc = Handle_CellDefault_Packed ( ncbi :: id ( evt_header ) );
            break;
            
        case evt_next_row:
            rc = HandleNextRow ( ncbi :: id ( evt_header ) );
            break;
            
        case evt_move_ahead:
            {
                uint32_t table_id = ncbi :: id ( evt_header );
                uint64_t count;
                rc = m_reader . Read ( & count, sizeof ( count ) );  
                if ( rc == 0 )
                {
                    rc = Handle_MoveAhead ( table_id, count );
                }
            }
            break;
            
        case evt_errmsg2:
            LogMsg ( klogInfo, "general-loader event: Error-Message" );
            {   
                uint16_t message_size;
                rc = m_reader . Read ( & message_size, sizeof ( message_size ) );
                if ( rc == 0 )
                {
                    ++message_size;
                    
                    rc = m_reader . Read ( message_size );
                    if ( rc == 0 )
                    {
                        rc = Handle_ErrorMessage ( string ( ( const char * ) m_reader . GetBuffer (), message_size ) );
                    }
                }
            }
            break;
            
        default:
            pLogMsg ( klogErr, "unexpected general-loader event: $(e)", "e=%i", ( int ) ncbi :: evt ( evt_header ) );
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        }
    }
    while ( rc == 0 );
    
    return rc;
}

rc_t 
GeneralLoader::Handle_UseSchema ( const string& p_file, const string& p_name )
{
    pLogMsg ( klogInfo, "general-loader: schema file '$(s1)', name '$(s2)'", "s1=%s,s2=%s", 
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
                          "general-loader: Added schema include path '$(s)'", 
                          "s=%s", 
                          it -> c_str() );
            }
            else if ( GetRCObject ( rc ) == (RCObject)rcPath )
            {
                pLogMsg ( klogInfo, 
                          "general-loader: Schema include path not found: '$(s)'", 
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
                              "general-loader: Added schema file '$(s)'", 
                              "s=%s", 
                              p_file . c_str () );
                    found = true;
                }
                else if ( GetRCObject ( rc ) == (RCObject)rcPath && GetRCState ( rc ) == rcNotFound )
                {
                    pLogMsg ( klogInfo, 
                              "general-loader: Schema file not found: '$(s)'", 
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
                                  "general-loader: Added schema file '$(s)'", 
                                  "s=%s", 
                                  it -> c_str() );
                        found = true;
                    }
                    else if ( GetRCObject ( rc ) == (RCObject)rcPath && GetRCState ( rc ) == rcNotFound )
                    {
                        pLogMsg ( klogInfo, 
                                  "general-loader: Schema file not found: '$(s)'", 
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
GeneralLoader::Handle_RemotePath ( const string& p_path )
{
    rc_t rc = VDBManagerCreateDB ( m_mgr, 
                                   & m_db, 
                                   m_schema, 
                                   m_schemaName . c_str (), 
                                   kcmInit + kcmMD5, 
                                   "%s", 
                                   p_path . c_str () );
    if ( rc == 0 )
    {
        pLogMsg ( klogInfo, 
                  "general-loader: Database created, schema spec='$(s)', database='$(d)'", 
                  "s=%s,d=%s", 
                  m_schemaName . c_str (), p_path . c_str () );
    }
    // set m_databaseName regardless of rc, so that it can be cleaned up later
    m_databaseName = p_path;
    return rc;
}

rc_t 
GeneralLoader::Handle_NewTable ( uint32_t p_tableId, const string& p_tableName )
{   
    VTable* table;
    rc_t rc = VDatabaseCreateTable ( m_db, & table, p_tableName . c_str (), kcmCreate | kcmMD5, "%s", p_tableName . c_str ());
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
    return rc;
}

rc_t 
GeneralLoader::Handle_NewColumn ( uint32_t p_columnId, uint32_t p_tableId, uint32_t p_elemBits, uint8_t p_flagBits, const string& p_columnName )
{
    pLogMsg ( klogInfo, "general-loader: adding column '$(c)'", "c=%s", p_columnName . c_str() );
    
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
                m_columns [ p_columnId ] = col;
                pLogMsg ( klogInfo, 
                          "general-loader: tableId = $(t), added column '$(c)', columnIdx = $(i1), elemBits = $(i2)",  
                          "t=%u,c=%s,i1=%u,i2=%u",  
                          p_tableId, p_columnName . c_str(), column_idx, p_elemBits );
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
GeneralLoader::Handle_CellData ( uint32_t p_columnId )
{
    pLogMsg ( klogInfo, "general-loader event: Cell-Data, id=$(i)", "i=%u", p_columnId );
    
    rc_t rc = 0;
    Columns::const_iterator curIt = m_columns . find ( p_columnId );
    if ( curIt != m_columns . end () )
    {
        const Column& col = curIt -> second;
        uint32_t elem_count;
        rc = m_reader . Read ( & elem_count, sizeof ( elem_count ) );   
        if ( rc == 0 )
        {
            pLogMsg ( klogInfo,     
                      "general-loader: columnIdx = $(i), elem size=$(s) bits, elem count=$(c)",
                      "i=%u,s=%u,c=%u", 
                      col . columnIdx, col . elemBits, elem_count );
            rc = m_reader . Read ( ( col . elemBits * elem_count + 7 ) / 8 );   
            if ( rc == 0 )
            {
                rc = VCursorWrite ( m_cursors [ col . cursorIdx ], 
                                    col . columnIdx, 
                                    col . elemBits, 
                                    m_reader . GetBuffer(), 
                                    0, 
                                    elem_count );
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
    }
    return rc;
}

rc_t 
GeneralLoader::Handle_CellDefault ( uint32_t p_columnId )
{   //TODO: this and Handle_CellData are almost identical - refactor
    pLogMsg ( klogInfo, "general-loader event: Cell-Default, id=$(i)", "i=%u", p_columnId );
    
    rc_t rc = 0;
    Columns::const_iterator curIt = m_columns . find ( p_columnId );
    if ( curIt != m_columns . end () )
    {
        const Column& col = curIt -> second;
        uint32_t elem_count;
        rc = m_reader . Read ( & elem_count, sizeof ( elem_count ) );   
        if ( rc == 0 )
        {
            pLogMsg ( klogInfo,     
                      "general-loader: columnIdx = $(i), elem size=$(s) bits, elem count=$(c)",
                      "i=%u,s=%u,c=%u", 
                      col . columnIdx, col . elemBits, elem_count );
            rc = m_reader . Read ( ( col . elemBits * elem_count + 7 ) / 8 );   
            if ( rc == 0 )
            {
                rc = VCursorDefault ( m_cursors [ col . cursorIdx ], 
                                      col . columnIdx, 
                                      col . elemBits, 
                                      m_reader . GetBuffer(), 
                                      0, 
                                      elem_count );
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
    }
    return rc;
}

rc_t 
GeneralLoader::Handle_CellData_Packed ( uint32_t p_columnId )
{
    pLogMsg ( klogInfo, "general-loader event: Cell-Data(Packed), id=$(i)", "i=%u", p_columnId );
    
    rc_t rc = 0;
    Columns::const_iterator curIt = m_columns . find ( p_columnId );
    if ( curIt != m_columns . end () )
    {
        const Column& col = curIt -> second;
        uint16_t data_size;
        rc = m_reader . Read ( & data_size, sizeof ( data_size ) );   
        if ( rc == 0 )
        {
            ++data_size;
            pLogMsg ( klogInfo,     
                      "general-loader: columnIdx = $(i), elem size=$(s) bits, elem count=$(c)",
                      "i=%u,s=%u,c=%u", 
                      col . columnIdx, col . elemBits, data_size * 8 / col . elemBits );
            rc = m_reader . Read ( data_size );   
            if ( rc == 0 )
            {
                rc = VCursorWrite ( m_cursors [ col . cursorIdx ], 
                                    col . columnIdx, 
                                    col . elemBits, 
                                    m_reader . GetBuffer(), 
                                    0, 
                                    data_size * 8 / col . elemBits );
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
    }
    return rc;
}

rc_t 
GeneralLoader::Handle_CellDefault_Packed ( uint32_t p_columnId )
{   //TODO: this and Handle_CellData_Packed are almost identical - refactor
    pLogMsg ( klogInfo, "general-loader event: Cell-Data(Packed), id=$(i)", "i=%u", p_columnId );
    
    rc_t rc = 0;
    Columns::const_iterator curIt = m_columns . find ( p_columnId );
    if ( curIt != m_columns . end () )
    {
        const Column& col = curIt -> second;
        uint16_t data_size;
        rc = m_reader . Read ( & data_size, sizeof ( data_size ) );   
        if ( rc == 0 )
        {
            ++data_size;
            pLogMsg ( klogInfo,     
                      "general-loader: columnIdx = $(i), elem size=$(s) bits, elem count=$(c)",
                      "i=%u,s=%u,c=%u", 
                      col . columnIdx, col . elemBits, data_size * 8 / col . elemBits );
            rc = m_reader . Read ( data_size );   
            if ( rc == 0 )
            {
                rc = VCursorDefault( m_cursors [ col . cursorIdx ], 
                                    col . columnIdx, 
                                    col . elemBits, 
                                    m_reader . GetBuffer(), 
                                    0, 
                                    data_size * 8 / col . elemBits );
            }
        }
    }
    else
    {
        rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
    }
    return rc;
}

rc_t 
GeneralLoader::OpenCursors ()
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
            return rc;
        }
    }
    return 0;
}

rc_t 
GeneralLoader::CloseCursors ()
{
    rc_t rc = 0;
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
                rc_t rc2 = VTableRelease ( table );
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
    return rc;
}

rc_t 
GeneralLoader::HandleNextRow ( uint32_t p_tableId )
{
    pLogMsg ( klogInfo, "general-loader event: Next-Row, id=$(i)", "i=%u", p_tableId );
    
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
GeneralLoader::Handle_MoveAhead ( uint32_t p_tableId, uint64_t p_count )
{
    pLogMsg ( klogInfo, "general-loader event: Repeat-Row, id=$(i), count=$(c)", "i=%u,c=%lu", p_tableId, p_count );
    
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
GeneralLoader::Handle_ErrorMessage ( const string & p_text )
{
    pLogMsg ( klogErr, 
              "general-loader event: Error-Message [$(s)] = \"$(t)\"",
              "s=%u,t=%s", 
              p_text . size (), 
              p_text . c_str () );
    return RC ( rcExe, rcFile, rcReading, rcError, rcExists );
}