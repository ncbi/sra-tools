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

/**
* Test stream creation class for unit-testing General Loader
*/

#include <sysalloc.h>

#include <kfc/defs.h>

#include <kfs/ramfile.h>

#include "../../tools/general-loader/general-loader.hpp"

#include <cstring>

#include <string>
#include <stdexcept> 
#include <fstream>

class TestSource
{
public:
    typedef uint32_t TableId;
    typedef uint32_t ColumnId;

public:
    TestSource( const std:: string& p_signature = GeneralLoaderSignatureString, 
                uint32_t p_endness = 1, 
                uint32_t p_version  = 1 )
    :   m_buffer(0), m_bufSize (0)
    {
        strncpy ( m_header . signature, p_signature . c_str(), sizeof m_header . signature );
        m_header . endian                   = p_endness;
        m_header . version                  = p_version;
        m_header . remote_db_name_size      = 0;
        m_header . schema_file_name_size    = 0;
        m_header . schema_spec_size         = 0;
    }
    
    ~TestSource()
    {
        free ( m_buffer );
    }
    
    void SetNames ( const std:: string& p_schemaFile, const std:: string& p_schemaSpec, const std:: string& p_databaseName )
    {
        m_header . remote_db_name_size      = p_databaseName . size();
        m_header . schema_file_name_size    = p_schemaFile . size();
        m_header . schema_spec_size         = p_schemaSpec . size();
        
        m_database      = p_databaseName;
        m_schemaFile    = p_schemaFile;
        m_schemaSpec    = p_schemaSpec;
    }
    
    const std::string & GetDatabaseName() { return m_database; }
    
    const struct KFile * MakeTruncatedSource ( size_t p_size )
    {
        FillBuffer();
        
        const struct KFile * ret;
        if ( KRamFileMakeRead ( & ret, m_buffer, p_size ) != 0 || ret == 0 )
            throw std :: logic_error("TestSource::MakeTruncatedSource KRamFileMakeRead failed");
            
        return ret;
    }
    
    const struct KFile * MakeSource ()
    {
        FillBuffer();
        
        const struct KFile * ret;
        if ( KRamFileMakeRead ( & ret, m_buffer, m_bufSize ) != 0 || ret == 0 )
            throw std :: logic_error("TestSource::MakeTruncatedSource KRamFileMakeRead failed");
            
        return ret;
    }
    
    void OpenStreamEvent ()
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_open_stream ) );
    }
    
    void CloseStreamEvent ()
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_end_stream ) );
    }
    
    void NewTableEvent ( TableId p_id, const char* p_table )
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_new_table, p_id, p_table ) );
    }
    
    void NewColumnEvent ( ColumnId p_columnId, TableId p_tableId, const char* p_column )
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_new_column, p_columnId, p_tableId, p_column ) );
    }
    
    void CellDefaultEvent ( ColumnId p_columnId, const std :: string& p_value )
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_cell_default, p_columnId, 8, p_value . size(), p_value . c_str() ) );
    }
    void CellDefaultEvent ( ColumnId p_columnId, uint32_t p_value )
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_cell_default, p_columnId, 8 * sizeof p_value, 1, &p_value ) );
    }
    void CellDefaultEvent ( ColumnId p_columnId, bool p_value )
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_cell_default, p_columnId, 8, 1, &p_value ) );
    }
    
    template < typename T > void CellDataEvent ( ColumnId p_columnId, T p_value )
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_cell_data, p_columnId, 8 * sizeof p_value, 1, &p_value ) );
    }

    void NextRowEvent ( TableId p_id )
    {
        m_events . push_back ( Event ( GeneralLoader :: evt_next_row, p_id ) );
    }
    
    // use this to capture stream contents to be used in cmdline tests
    void SaveBuffer( const char* p_filename )
    {
        std :: ofstream ( p_filename ) . write ( m_buffer, m_bufSize ); 
    }
    
private:    
    void Write( const void * p_data, size_t p_size )
    {
        m_buffer = ( char * ) realloc ( m_buffer, m_bufSize + p_size );
        if ( m_buffer == 0 )
            throw std :: logic_error("TestSource::Write: realloc failed");
            
        memcpy ( m_buffer + m_bufSize, p_data, p_size );
        m_bufSize += p_size;
    }
    
    void Pad ( size_t p_alignment = 4 )
    {
        size_t newSize = ( m_bufSize + p_alignment - 1 ) / p_alignment;
        newSize *= p_alignment;
        if ( m_bufSize != newSize ) 
        {
            m_buffer = ( char * ) realloc ( m_buffer, newSize );
            if ( m_buffer == 0 )
                throw std :: logic_error("TestSource::Write: realloc failed");
            memset ( m_buffer + m_bufSize, 0, newSize - m_bufSize );
            m_bufSize = newSize;
        }
    }
    
    void MakeHeader()
    {
        Write ( & m_header, sizeof m_header );
        Write ( m_database . c_str(), m_header . remote_db_name_size + 1 );
        Write ( m_schemaFile . c_str(), m_header . schema_file_name_size + 1 );
        Write ( m_schemaSpec . c_str(), m_header . schema_spec_size + 1 );
    }

public:
    void FillBuffer()
    {
        MakeHeader();
        Pad ();
        
        for ( Events :: const_iterator it = m_events . begin(); it != m_events . end(); ++it )
        {
            switch ( it -> m_event )
            {
            case GeneralLoader :: evt_open_stream:
            case GeneralLoader :: evt_end_stream:
                {
                    GeneralLoader :: Evt_hdr hdr;
                    hdr . evt   = it -> m_event;
                    hdr . id    = 0;
                    Write ( & hdr, sizeof hdr );
                    break;
                }
            case GeneralLoader :: evt_new_table :
                {
                    GeneralLoader ::  Table_hdr hdr;
                    hdr . evt   = it -> m_event;
                    hdr . id    = it -> m_id1;
                    hdr . table_name_size = it -> m_str . size ();
                    Write ( & hdr, sizeof hdr );
                    Write ( it -> m_str . c_str (), it -> m_str . size () + 1 );
                    break;
                }
            case GeneralLoader :: evt_new_column :
                {
                    GeneralLoader ::  Column_hdr hdr;
                    hdr . evt               = it -> m_event;
                    hdr . id                = it -> m_id1;
                    hdr . table_id          = it -> m_id2;
                    hdr . column_name_size  = it -> m_str . size ();
                    Write ( & hdr, sizeof hdr );
                    Write ( it -> m_str . c_str (), it -> m_str . size () + 1 );
                    break;
                }
            case GeneralLoader :: evt_cell_data :
            case GeneralLoader :: evt_cell_default :
                {
                    GeneralLoader ::  Cell_hdr hdr;
                    hdr . evt               = it -> m_event;
                    hdr . id                = it -> m_id1;
                    hdr . elem_bits         = it -> m_uint1;
                    hdr . elem_count        = it -> m_uint2;
                    Write ( & hdr, sizeof hdr );
                    Write ( it -> m_val . data(), it -> m_val . size() );
                    break;
                }
            case GeneralLoader :: evt_next_row:
                {
                    GeneralLoader :: Evt_hdr hdr;
                    hdr . evt   = it -> m_event;
                    hdr . id    = it -> m_id1;
                    Write ( & hdr, sizeof hdr );
                    break;
                }
            default:
                throw std :: logic_error("TestSource::FillBuffer: event not implemented");
            }
            Pad ();
        }
    }
    
private:    
    struct Event 
    {
        Event ( GeneralLoader :: Evt_id p_event )
        :   m_event ( p_event ),
            m_id1 ( 0 ),
            m_id2 ( 0 ),
            m_uint1 ( 0 ),
            m_uint2 ( 0 )
        {
        }
        
        Event ( GeneralLoader :: Evt_id p_event, uint32_t p_id1 )
        :   m_event ( p_event ),
            m_id1 ( p_id1 ),
            m_id2 ( 0 ),
            m_uint1 ( 0 ),
            m_uint2 ( 0 )
        {
        }
        
        Event ( GeneralLoader :: Evt_id p_event, uint32_t p_id, const char* p_str )
        :   m_event ( p_event ),
            m_id1 ( p_id ),
            m_id2 ( 0 ),
            m_uint1 ( 0 ),
            m_uint2 ( 0 ),
            m_str ( p_str )
        {
        }
        
        Event ( GeneralLoader :: Evt_id p_event, uint32_t p_id1, uint32_t p_id2, const char* p_str )
        :   m_event ( p_event ),
            m_id1 ( p_id1 ),
            m_id2 ( p_id2 ),
            m_uint1 ( 0 ),
            m_uint2 ( 0 ),
            m_str ( p_str )
        {
        }
        
        Event ( GeneralLoader :: Evt_id p_event, uint32_t p_id1, uint32_t p_uint1, uint32_t p_uint2, const void* p_val )
        :   m_event ( p_event ),
            m_id1 ( p_id1 ),
            m_id2 ( 0 ),
            m_uint1 ( p_uint1 ),
            m_uint2 ( p_uint2 )
        {
            size_t bytes = ( p_uint1 * p_uint2 + 7 ) / 8;
            const char* v = ( const char* ) p_val;
            for ( size_t i = 0; i != bytes; ++i )
            {
                m_val . push_back ( v [ i ] );
            }
        }
        
        ~Event()
        {
        }
        
        GeneralLoader :: Evt_id m_event;
        uint32_t                m_id1;
        uint32_t                m_id2;
        uint32_t                m_uint1;
        uint32_t                m_uint2;
        std :: string           m_str;
        std :: vector < char >  m_val;
    };
    typedef std :: vector < Event > Events;
    
    std :: string m_database;
    std :: string m_schemaFile;
    std :: string m_schemaSpec;
    
    GeneralLoader :: Header m_header;
    Events                  m_events;
    
    char*   m_buffer;
    size_t  m_bufSize;
};

template<> void TestSource::CellDataEvent ( ColumnId p_columnId, std :: string p_value )
{
    m_events . push_back ( Event ( GeneralLoader :: evt_cell_data, p_columnId, 8, p_value . size(), p_value . c_str() ) );
}

