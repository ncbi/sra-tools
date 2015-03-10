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
}

GeneralLoader::Reader::~Reader()
{
    free ( m_buffer );
}

rc_t 
GeneralLoader::Reader::Read( void * p_buffer, size_t p_size )
{
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
    m_headerNames ( 0 ),
    m_db ( 0 )
{
}

GeneralLoader::~GeneralLoader ()
{
    Reset();
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
    }
    m_db = 0;
    
    free ( m_headerNames );
    m_headerNames = 0;
}

void 
GeneralLoader::CleanUp()
{   // remove the database
    if ( m_headerNames != 0 )
    {
        string dbName ( & m_headerNames [ 0 ] );
        Reset();
        
        KDirectory* wd;
        KDirectoryNativeDir ( & wd );
        KDirectoryRemove ( wd, true, dbName. c_str () );
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
        rc = MakeDatabase ();
        if ( rc == 0 )
        {
            rc = ReadMetadata ();
            if ( rc == 0 ) 
            {
                rc = OpenCursors ();
                if ( rc == 0 )
                {
                    rc = ReadData ();
                }
            }
        }
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
        if ( strncmp ( m_header . signature, GeneralLoaderSignatureString, sizeof ( m_header . signature ) ) != 0 )
        {
            rc = RC ( rcExe, rcFile, rcReading, rcHeader, rcCorrupt );
        }
        else 
        {
            switch ( m_header . endian )
            {
            case 1:
                if ( m_header . version != 1 )
                {
                    rc = RC ( rcExe, rcFile, rcReading, rcHeader, rcBadVersion );
                }
                else
                {
                    rc = 0;
                }
                break;
            case 0x10000000:
                rc = RC ( rcExe, rcFile, rcReading, rcFormat, rcUnsupported );
                //TODO: apply byte order correction before checking the version number
                break;
            default:
                rc = RC ( rcExe, rcFile, rcReading, rcFormat, rcInvalid );
                break;
            }
        }
    }
    
    if ( rc == 0 )
    {   
        size_t size = m_header . remote_db_name_size + 1 + m_header . schema_file_name_size + 1 + m_header . schema_spec_size + 1;
        m_headerNames = ( char * ) calloc ( 1, size );
        if ( m_headerNames != 0 )
        {
            rc = m_reader . Read ( m_headerNames, size );
        }
        else
        {
            rc = RC ( rcExe, rcFile, rcReading, rcMemory, rcExhausted );
        }
    }
    return rc;
}

rc_t 
GeneralLoader :: ReadMetadata ()
{
    rc_t rc;
    do 
    { 
        m_reader . Align ();
        
        Evt_hdr evt_header;
        rc = m_reader . Read ( & evt_header, sizeof ( evt_header ) );    
        if ( rc != 0 )
        {
            break;
        }
        
        switch ( evt_header . evt )
        {
        case evt_open_stream:
            LogMsg ( klogInfo, "general-loader event: Open-Stream" );
            return 0; // end of metadata
        case evt_end_stream:
            LogMsg ( klogErr, "unexpected general-loader event: End-Stream" );
            // premature end of stream
            rc = RC ( rcExe, rcFile, rcReading, rcTransfer, rcCanceled );
            break;
        case evt_new_table:
            pLogMsg ( klogInfo, "general-loader event: New-Table, id=$(i)", "i=%u", evt_header . id );
            {   
                if ( m_tables . find ( evt_header . id ) == m_tables . end() )
                {
                    uint32_t table_name_size;
                    rc = m_reader . Read ( & table_name_size, sizeof ( table_name_size ) );
                    if ( rc == 0 )
                    {
                        rc = m_reader . Read ( table_name_size + 1 );
                        if ( rc == 0 )
                        {
                            pLogMsg ( klogInfo, "general-loader: adding table '$(c)'", "c=%s", ( const char * ) m_reader . GetBuffer () );
                            rc = MakeCursor ( ( char * ) m_reader . GetBuffer() );
                            if ( rc == 0 )
                            {
                                m_tables [ evt_header . id ] = m_cursors . size() - 1;
                            }
                        }
                        
                    }
                }
                else
                {
                    rc = RC ( rcExe, rcFile, rcReading, rcTable, rcExists );
                }
                break;
            }
        case evt_new_column:
            pLogMsg ( klogInfo, "general-loader event: New-Column, id=$(i)", "i=%u", evt_header . id );
            {   
                uint32_t table_id;
                rc = m_reader . Read ( & table_id , sizeof ( table_id ) );
                if ( rc == 0 )
                {
                    TableIdToCursor::const_iterator table = m_tables . find ( table_id );
                    if ( table != m_tables . end() )
                    {
                        if ( m_columns . find ( evt_header . id ) == m_columns . end () )
                        {
                            uint32_t col_name_size;
                            rc = m_reader . Read ( & col_name_size, sizeof ( col_name_size ) );
                            if ( rc == 0 )
                            {
                                rc = m_reader . Read ( col_name_size + 1 );
                                if ( rc == 0 )
                                {
                                    pLogMsg ( klogInfo, "general-loader: adding column '$(c)'", "c=%s", ( const char * ) m_reader . GetBuffer () );
                                    uint32_t cursorIdx = table -> second;
                                    uint32_t columnIdx;
                                    rc = VCursorAddColumn ( m_cursors [ cursorIdx ], 
                                                            & columnIdx, 
                                                            "%s", 
                                                            ( const char * ) m_reader . GetBuffer () );
                                    if ( rc == 0  )
                                    {
                                        m_columns [ evt_header . id ] = ColumnToCursor :: mapped_type ( cursorIdx, columnIdx );
                                        pLogMsg ( klogInfo, 
                                                  "general-loader: tableId = $(t), added column '$(c)', columnIdx = $(i)", 
                                                  "t=%u,c=%s,i=%u",  
                                                  table_id, ( const char * ) m_reader . GetBuffer (), columnIdx );
                                    }
                                }
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
                }
                break;
            }
        case evt_cell_default:
            LogMsg ( klogErr, "unexpected general-loader event: Cell-Default");
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        case evt_cell_data:
            LogMsg ( klogErr, "unexpected general-loader event: Cell-Data");
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        case evt_next_row:
            LogMsg ( klogErr, "unexpected general-loader event: Next-Row");
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        default:
            pLogMsg ( klogErr, "unexpected general-loader event: $(e)", "e=%u", evt_header . evt );
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        }
    }
    while ( rc == 0 );
    
    return rc;
}

rc_t 
GeneralLoader::MakeDatabase ()
{
    VDBManager* mgr;
    rc_t rc = VDBManagerMakeUpdate ( & mgr, NULL );
    if ( rc == 0 )
    {
        VSchema* schema;
        rc = VDBManagerMakeSchema ( mgr, & schema );
        if ( rc  == 0 )
        {
            const char * schemaFile = & m_headerNames [ m_header . remote_db_name_size + 1 ];
            rc = VSchemaParseFile(schema, "%s", schemaFile );
            if ( rc  == 0 )
            {
                const char * schemaSpec = & m_headerNames [ m_header . remote_db_name_size + 1 + 
                                                            m_header . schema_file_name_size + 1 ];
                const char * databaseName = & m_headerNames [ 0 ];
                rc = VDBManagerCreateDB ( mgr, 
                                          & m_db, 
                                          schema, 
                                          schemaSpec, 
                                          kcmInit + kcmMD5, 
                                          "%s", 
                                          databaseName );
                if ( rc == 0 )
                {
                    pLogMsg ( klogInfo, 
                              "general-loader: Database created, schema='$(s)', spec='$(p)', database='$(d)'", 
                              "s=%s,p=%s,d=%s", 
                              schemaFile, schemaSpec, databaseName );
                
                    rc_t rc2 = VSchemaRelease ( schema );
                    if ( rc == 0 )
                        rc = rc2;
                    rc2 = VDBManagerRelease ( mgr );
                    if ( rc == 0 )
                        rc = rc2;
                    return rc;
                }
            }
            VSchemaRelease ( schema );
        }
        VDBManagerRelease ( mgr );
    }
    return rc;
}

rc_t 
GeneralLoader::MakeCursor ( const char* p_tableName )
{   // new cursor means new table
    VTable* table;
    rc_t rc = VDatabaseCreateTable ( m_db, & table, p_tableName, kcmCreate | kcmMD5, "%s", p_tableName );
    if ( rc == 0 )
    {
        VCursor* cursor;
        rc = VTableCreateCursorWrite ( table, & cursor, kcmInsert );
        if ( rc == 0 )
        {
            m_cursors . push_back ( cursor );
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
GeneralLoader::ReadData ()
{
    rc_t rc;
    do 
    { 
        m_reader . Align ();
        
        Evt_hdr evt_header;
        rc = m_reader . Read ( & evt_header, sizeof ( evt_header ) );    
        if ( rc != 0 )
        {
            break;
        }
        
        switch ( evt_header . evt )
        {
        case evt_end_stream:
            LogMsg ( klogInfo, "general-loader event: End-Stream" );
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
            return rc; // end of input
            
        case evt_cell_default:
            pLogMsg ( klogInfo, "general-loader event: Cell-Default, id=$(i)", "i=%u", evt_header . id );
            {
                ColumnToCursor::iterator curIt = m_columns . find ( evt_header . id );
                if ( curIt != m_columns . end () )
                {
                    uint32_t elem_bits;
                    rc = m_reader . Read ( & elem_bits, sizeof ( elem_bits ) );    
                    if ( rc == 0 )
                    {
                        uint32_t elem_count;
                        rc = m_reader . Read ( & elem_count, sizeof ( elem_count ) );   
                        if ( rc == 0 )
                        {
                            rc = m_reader . Read ( ( elem_bits * elem_count + 7 ) / 8 );   
                            if ( rc == 0 )
                            {
                                VCursor * cursor = m_cursors [ curIt -> second . first ];
                                uint32_t colIdx = curIt -> second . second;
                                rc = VCursorDefault ( cursor, colIdx, elem_bits, m_reader . GetBuffer(), 0, elem_count );
                                pLogMsg ( klogInfo,     
                                          "general-loader: columnIdx = $(i), default value's size=$(s) bits", 
                                          "i=%u,s=%u", 
                                           colIdx, elem_bits * elem_count );
                            }
                        }
                    }
                }
                else
                {
                    rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
                }
            }
            break;
        case evt_cell_data:
            pLogMsg ( klogInfo, "general-loader event: Cell-Data, id=$(i)", "i=%u", evt_header . id );
            {
                ColumnToCursor::iterator curIt = m_columns . find ( evt_header . id );
                if ( curIt != m_columns . end () )
                {
                    uint32_t elem_bits;
                    rc = m_reader . Read ( & elem_bits, sizeof ( elem_bits ) );    
                    if ( rc == 0 )
                    {
                        uint32_t elem_count;
                        rc = m_reader . Read ( & elem_count, sizeof ( elem_count ) );   
                        if ( rc == 0 )
                        {
                            rc = m_reader . Read ( ( elem_bits * elem_count + 7 ) / 8 );   
                            if ( rc == 0 )
                            {
                                VCursor * cursor = m_cursors [ curIt -> second . first ];
                                uint32_t colIdx = curIt -> second . second;
                                rc = VCursorWrite ( cursor, colIdx, elem_bits, m_reader . GetBuffer(), 0, elem_count );
                                pLogMsg ( klogInfo,     
                                          "general-loader: columnIdx = $(i), value's size=$(s) bits", 
                                          "i=%u,s=%u", 
                                           colIdx, elem_bits * elem_count );
                            }
                        }
                    }
                }
                else
                {
                    rc = RC ( rcExe, rcFile, rcReading, rcColumn, rcNotFound );
                }
            }
            break;
        case evt_next_row:
            pLogMsg ( klogInfo, "general-loader event: Next-Row, id=$(i)", "i=%u", evt_header . id );
            {
                TableIdToCursor::const_iterator table = m_tables . find ( evt_header . id );
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
            }
            break;
            
        case evt_open_stream:
            LogMsg ( klogErr, "unexpected general-loader event: Open-Stream");
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        case evt_new_table:
            LogMsg ( klogErr, "unexpected general-loader event: New-Table");
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        case evt_new_column:
            LogMsg ( klogErr, "unexpected general-loader event: New-Column");
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        default:
            pLogMsg ( klogErr, "unexpected general-loader event: $(e)", "e=%u", evt_header . evt );
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcUnexpected );
            break;
        }
    }
    while ( rc == 0 );
    
    return rc;
}


