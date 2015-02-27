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

#include <kns/stream.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/cursor.h>
#include <vdb/table.h>

#include <cstring>

using namespace std;

GeneralLoader::GeneralLoader ( const struct KStream& p_input )
:   m_input ( p_input ),
    m_columns (0),
    m_strings(0),
    m_db(0)
{
}

GeneralLoader::~GeneralLoader ()
{
    Reset();
}

void
GeneralLoader::Reset()
{
    free ( m_columns );
    m_columns = 0;
    free ( m_strings );
    m_strings = 0;
    
    for ( Cursors::iterator it = m_cursors . begin(); it != m_cursors . end(); ++it )
    {
        VCursorRelease ( it -> second );
    }
    
    VDatabaseRelease ( m_db );
}

rc_t 
GeneralLoader::Run()
{
    Reset();
    
    rc_t rc = ReadHeader ();
    if ( rc != 0 ) return rc; 
    
    rc = ReadMetadata ();
    if ( rc != 0 ) return rc; 
    
    rc = ReadColumns ();
    if ( rc != 0 ) return rc; 
    
    rc = ReadStringTable ();
    if ( rc != 0 ) return rc; 
    
    rc = MakeDatabase ();
    if ( rc != 0 ) return rc; 
    
    rc = MakeCursors ();
    if ( rc != 0 ) return rc; 
    
    rc = OpenCursors ();
    if ( rc != 0 ) return rc; 
    
    rc = ReadData ();
    if ( rc != 0 ) return rc; 
    
    return rc;
}
 
rc_t 
GeneralLoader::ReadHeader ()
{
    Header header;
    size_t num_read;
    rc_t rc = KStreamRead ( & m_input, & header, sizeof header, & num_read );
    if ( rc == 0 )
    { 
        if ( num_read == sizeof header )
        {
            if ( strncmp ( header . signature, GeneralLoaderSignatureString, sizeof ( header . signature ) ) != 0 )
            {
                rc = RC ( rcExe, rcFile, rcReading, rcHeader, rcCorrupt );
            }
            else 
            {
                switch ( header . endian )
                {
                case 1:
                    if ( header . version != 1 )
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
                    break;
                default:
                    rc = RC ( rcExe, rcFile, rcReading, rcFormat, rcInvalid );
                    break;
                }
            }
        }
        else
        {
            rc = RC ( rcExe, rcFile, rcReading, rcFile, rcInsufficient );
        }
    }
    return rc;
}

rc_t 
GeneralLoader :: ReadMetadata ()
{
    size_t num_read;
    rc_t rc = KStreamRead ( & m_input, & m_md, sizeof m_md, & num_read );
    if ( rc != 0 ) 
    {
        return rc;
    }
    if ( num_read != sizeof m_md )
    {
        return RC ( rcExe, rcFile, rcReading, rcMetadata, rcCorrupt );
    }
    return 0;
}

rc_t 
GeneralLoader :: ReadColumns ()
{
    size_t columnsSize = m_md . num_columns * sizeof ( Column );
    if ( m_md . md_size != sizeof ( m_md ) + columnsSize + m_md . str_size )
    {
        return RC ( rcExe, rcFile, rcReading, rcMetadata, rcInvalid );
    }

    if ( columnsSize == 0 )
    {   // no columns: weird but valid ?
        return 0;
    }
    
    m_columns = ( Column * ) malloc ( columnsSize );
    if ( m_columns == 0 ) 
    {
        return RC ( rcExe, rcFile, rcReading, rcMemory, rcExhausted );
    }
    
    size_t num_read;
    rc_t rc = KStreamRead ( & m_input, m_columns, columnsSize, & num_read );
    if ( rc != 0 ) 
    {
        return rc;
    }    
    if ( num_read != columnsSize )
    {
        return RC ( rcExe, rcFile, rcReading, rcMetadata, rcIncomplete );
    }
    return 0;
}

rc_t 
GeneralLoader::ReadStringTable()
{    
    if ( m_md . str_size == 0 )
    {
        return RC ( rcExe, rcFile, rcReading, rcMetadata, rcInvalid );
    }
    
    m_strings = ( char * ) malloc ( m_md . str_size );
    if ( m_strings == 0 ) 
    {
        return RC ( rcExe, rcFile, rcReading, rcMemory, rcExhausted );
    }
    
    size_t num_read;
    rc_t rc = KStreamRead ( & m_input, m_strings, m_md . str_size, & num_read );
    if ( rc != 0 ) 
    {
        return rc;
    }    
    if ( num_read != m_md . str_size )
    {
        return RC ( rcExe, rcFile, rcReading, rcMetadata, rcIncomplete );
    }
    
    return 0;
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
            rc = VSchemaParseFile(schema, "%s", & m_strings [ m_md . schema_file_offset ] );
            if ( rc  == 0 )
            {
                rc = VDBManagerCreateDB ( mgr, 
                                          & m_db, 
                                          schema, 
                                          & m_strings [ m_md . schema_offset ], 
                                          kcmInit + kcmMD5, 
                                          "%s", 
                                          & m_strings [ m_md . database_name_offset ] );
                if ( rc == 0 )
                {
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
GeneralLoader::MakeCursor ( const char * p_tableName )
{   // new cursor means new table
    VTable* table;
    rc_t rc = VDatabaseCreateTable ( m_db, & table, p_tableName, kcmCreate | kcmMD5, "%s", p_tableName );
    if ( rc == 0 )
    {
        VCursor* cursor;
        rc = VTableCreateCursorWrite ( table, & cursor, kcmInsert );
        if ( rc == 0 )
        {
            m_cursors . push_back ( Cursors::value_type ( string ( p_tableName ), cursor ));
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
GeneralLoader::MakeCursors ()
{
    rc_t rc;
    for ( uint32_t i = 0; i < m_md . num_columns; ++i )
    {
        //TODO: verify offsets
        string tableName    = & m_strings [ m_columns [ i ] . table_name_offset ];
        string columnName   = & m_strings [ m_columns [ i ] . column_name_offset ];
        size_t cursorIdx;
        for ( cursorIdx = 0; cursorIdx != m_cursors . size(); ++ cursorIdx )
        {
            if ( m_cursors [ cursorIdx ] . first == tableName )
            {
                break;
            }
        }
        
        if ( cursorIdx == m_cursors . size() ) 
        {   
            rc = MakeCursor ( tableName . c_str() );
            if ( rc != 0 )
            {
                return rc;
            }
            cursorIdx = m_cursors . size () - 1;
        }
        
        // add the column ( OK if already exists )
        uint32_t columnIdx;
        rc = VCursorAddColumn ( m_cursors [ cursorIdx ] . second, & columnIdx, "%s", columnName . c_str() );
        if ( rc != 0 && 
            ( ( int ) GetRCObject( rc ) != rcColumn || GetRCState ( rc ) != rcExists ) )
        {
            return rc;
        }

        m_columnToCursor . push_back ( ColumnToCursor :: value_type ( cursorIdx, columnIdx ) );
    }
    return 0;
}

rc_t 
GeneralLoader::OpenCursors ()
{
    for ( Cursors::iterator it = m_cursors . begin(); it != m_cursors . end(); ++it )
    {
        rc_t rc = VCursorOpen ( it -> second );
        if ( rc != 0 )
        {
            return rc;
        }
        rc = VCursorOpenRow ( it -> second );
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
    uint32_t stream_id;
    size_t num_read;
    rc_t rc = KStreamRead ( & m_input, & stream_id, sizeof stream_id, & num_read );
    if ( rc == 0 )
    { 
        if ( num_read == sizeof stream_id )
        {
            if ( stream_id == 0 )
            {   // end marker
                for ( Cursors::iterator it = m_cursors . begin(); it != m_cursors . end(); ++it )
                {
                    rc_t rc = VCursorCloseRow ( it -> second );
                    if ( rc != 0 )
                    {
                        return rc;
                    }
                    // VCursorRelease will be called from Reset() (e.g. invoked from the destructor)
                }
            }
            else
            {   // TBD
                rc = RC ( rcExe, rcFile, rcReading, rcData, rcInvalid );
            }
        }
        else
        {
            rc = RC ( rcExe, rcFile, rcReading, rcData, rcInsufficient );
        }
    }
    return rc;
}


