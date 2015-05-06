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

#ifndef _sra_tools_test_hpp_testsource
#define _sra_tools_test_hpp_testsource

/**
* Test stream creation class for unit-testing General Loader
*/

#include <sysalloc.h>

#include <kfc/defs.h>

#include <kfs/ramfile.h>

#include "../../tools/general-loader/general-writer.h"

#include <cstring>

#include <string>
#include <vector>
#include <stdexcept> 
#include <fstream>

class TestSource
{
public:
    typedef uint32_t TableId;
    typedef uint32_t ColumnId;

public:
    TestSource( const std:: string& p_signature = GW_SIGNATURE, 
                uint32_t p_endness = GW_GOOD_ENDIAN, 
                uint32_t p_version  = GW_CURRENT_VERSION )
    :   m_buffer(0), m_bufSize (0)
    {
        ncbi :: init ( m_header );
        strncpy ( m_header . dad . signature, p_signature . c_str(), sizeof m_header . dad . signature );
        m_header . dad . endian   = p_endness;
        m_header . dad . version  = p_version;
        m_header . packing = 0; //TODO: ad support for packing
    }
    
    ~TestSource()
    {
        free ( m_buffer );
    }
    
    const std::string & GetDatabaseName() { return m_database; }

#if 0    
    const struct KFile * MakeTruncatedSource ( size_t p_size )
    {
        FillBuffer();
        
        const struct KFile * ret;
        if ( KRamFileMakeRead ( & ret, m_buffer, p_size ) != 0 || ret == 0 )
            throw std :: logic_error("TestSource::MakeTruncatedSource KRamFileMakeRead failed");
            
        return ret;
    }
#endif
    
    const struct KFile * MakeSource ()
    {
        FillBuffer();
        
        const struct KFile * ret;
        if ( KRamFileMakeRead ( & ret, m_buffer, m_bufSize ) != 0 || ret == 0 )
            throw std :: logic_error("TestSource::MakeTruncatedSource KRamFileMakeRead failed");
            
        return ret;
    }
    
    void SchemaEvent ( const std::string& p_schemaFile, const std::string& p_schemaName )
    {
        m_events . push_back ( Event ( evt_use_schema, 0, p_schemaFile, p_schemaName ) );
    }
    
    void DatabaseEvent ( const std::string& p_databaseName )
    {
        m_events . push_back ( Event ( evt_remote_path, 0, p_databaseName ) );
        m_database = p_databaseName;
    }
    
    void NewTableEvent ( TableId p_id, const std::string& p_table )
    {
        m_events . push_back ( Event ( evt_new_table, p_id, p_table ) );
    }
    
    void NewColumnEvent ( ColumnId p_columnId, TableId p_tableId, const std::string& p_column, uint32_t p_elemBits )
    {
        m_events . push_back ( Event ( evt_new_column, p_columnId, p_tableId, p_column, p_elemBits ) );
        //TODO: support integer compaction
    }
        
    void OpenStreamEvent ()
    {
        m_events . push_back ( Event ( evt_open_stream ) );
    }
    
    void CloseStreamEvent ()
    {
        m_events . push_back ( Event ( evt_end_stream ) );
    }
    
    void NextRowEvent ( TableId p_id )
    {
        m_events . push_back ( Event ( evt_next_row, p_id ) );
    }
    
    template < typename T > void CellDataEvent ( ColumnId p_columnId, T p_value )
    {
        m_events . push_back ( Event ( evt_cell_data, p_columnId, 1, sizeof p_value, &p_value ) );
    }   // see below for specialization for std::string (follows the class definition)

    void CellDefaultEvent ( ColumnId p_columnId, const std :: string& p_value )
    {
        m_events . push_back ( Event ( evt_cell_default, p_columnId, p_value . size(), p_value . size(), p_value . c_str() ) );
    }
    void CellDefaultEvent ( ColumnId p_columnId, uint32_t p_value )
    {
        m_events . push_back ( Event ( evt_cell_default, p_columnId, 1,  sizeof p_value, (const void*)&p_value ) );
    }
    void CellDefaultEvent ( ColumnId p_columnId, bool p_value )
    {
        m_events . push_back ( Event ( evt_cell_default, p_columnId, 1, sizeof p_value, (const void*)&p_value ) );
    }
    
    void ErrorMessageEvent ( const std :: string& p_msg )
    {
        m_events . push_back ( Event ( evt_errmsg, 0, p_msg . c_str () ) );
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
    
public:
    void FillBuffer()
    {
        Write ( & m_header, m_header . dad . hdr_size ); 
        
        for ( Events :: const_iterator it = m_events . begin(); it != m_events . end(); ++it )
        {
            Pad ();
            switch ( it -> m_event )
            {
            case evt_use_schema:
                {   
                    gw_2string_evt_v1 hdr;
                    ncbi :: init ( hdr, it -> m_id1, it -> m_event );
                    ncbi :: set_size1 ( hdr, it -> m_str1 . size() );
                    ncbi :: set_size2 ( hdr, it -> m_str2 . size() );
                    
                    Write ( & hdr, sizeof hdr ); 
                    Write ( it -> m_str1 . c_str(), it -> m_str1 . size() ); 
                    Write ( it -> m_str2 . c_str(), it -> m_str2 . size() ); 
                }
                break;
                
            case evt_remote_path:
            case evt_new_table:
            case evt_errmsg:
                {   
                    gw_1string_evt_v1 hdr;
                    ncbi :: init ( hdr, it -> m_id1, it -> m_event );
                    ncbi :: set_size ( hdr, it -> m_str1 . size () );
                    
                    Write ( & hdr, sizeof hdr ); 
                    Write ( it -> m_str1 . c_str(), it -> m_str1 . size() ); 
                }
                break;
                
            case evt_new_column :
                {
                    gw_column_evt_v1 hdr;
                    ncbi :: init ( hdr, it -> m_id1, it -> m_event );
                    ncbi :: set_table_id ( hdr, it -> m_id2 );
                    ncbi :: set_elem_bits ( hdr, it -> m_uint );
                    ncbi :: set_name_size ( hdr, it -> m_str1 . size () );
                    
                    Write ( & hdr, sizeof hdr );
                    Write ( it -> m_str1 . c_str (), it -> m_str1 . size () );
                }
                break;
                
            case evt_open_stream:
            case evt_end_stream:
                {
                    gw_evt_hdr_v1 hdr;
                    ncbi :: init ( hdr, 0, it -> m_event );
                    Write ( & hdr, sizeof hdr );
                }
                break;

            case evt_cell_data :
            case evt_cell_default :
                {
                    gw_data_evt_v1 hdr;
                    ncbi :: init ( hdr, it -> m_id1, it -> m_event );
                    ncbi :: set_elem_count ( hdr, it -> m_uint );
                    
                    Write ( & hdr, sizeof hdr );
                    Write ( it -> m_val . data(), it -> m_val . size() );
                }
                break;
                
            case evt_next_row:
                {
                    gw_evt_hdr_v1 hdr;
                    ncbi :: init ( hdr, it -> m_id1, it -> m_event );
                    Write ( & hdr, sizeof hdr );
                }
                break;
                
            default:
                throw std :: logic_error("TestSource::FillBuffer: event not implemented");
            }
        }
    }
    
private:    
    struct Event 
    {
        Event ( gw_evt_id p_event )
        :   m_event ( p_event ),
            m_id1 ( 0 ),
            m_id2 ( 0 ),
            m_uint ( 0 )
        {
        }
        
        Event ( gw_evt_id p_event, uint32_t p_id1 )
        :   m_event ( p_event ),
            m_id1 ( p_id1 ),
            m_id2 ( 0 ),
            m_uint ( 0 )
        {
        }
        
        Event ( gw_evt_id p_event, uint32_t p_id, const char* p_str )
        :   m_event ( p_event ),
            m_id1 ( p_id ),
            m_id2 ( 0 ),
            m_uint ( 0 ),
            m_str1 ( p_str )
        {
        }
        
        Event ( gw_evt_id p_event, uint32_t p_id1, uint32_t p_id2, const std::string& p_str, uint32_t p_uint1 = 0 )
        :   m_event ( p_event ),
            m_id1 ( p_id1 ),
            m_id2 ( p_id2 ),
            m_uint ( p_uint1 ),
            m_str1 ( p_str )
        {
        }
        
        Event ( gw_evt_id p_event, uint32_t p_id, const std::string& p_str1 )
        :   m_event ( p_event ),
            m_id1 ( p_id ),
            m_id2 ( 0 ),
            m_uint ( 0 ),
            m_str1 ( p_str1 )
        {
        }
        
        Event ( gw_evt_id p_event, uint32_t p_id, const std::string& p_str1, const std::string& p_str2 )
        :   m_event ( p_event ),
            m_id1 ( p_id ),
            m_id2 ( 0 ),
            m_uint ( 0 ),
            m_str1 ( p_str1 ),
            m_str2 ( p_str2 )
        {
        }
        
        Event ( gw_evt_id p_event, uint32_t p_id1, uint32_t p_elem_count, uint32_t p_val_bytes, const void* p_val )
        :   m_event ( p_event ),
            m_id1 ( p_id1 ),
            m_id2 ( 0 ),
            m_uint ( p_elem_count )
        {
            const char* v = ( const char* ) p_val;
            for ( size_t i = 0; i != p_val_bytes; ++i )
            {
                m_val . push_back ( v [ i ] );
            }
        }
        
        ~Event()
        {
        }
        
        gw_evt_id               m_event;
        uint32_t                m_id1;
        uint32_t                m_id2;
        uint32_t                m_uint;
        std :: string           m_str1;
        std :: string           m_str2;
        std :: vector < char >  m_val;
    };
    typedef std :: vector < Event > Events;

    std :: string m_database;
#if 0
    std :: string m_schemaFile;
    std :: string m_schemaSpec;
    
#endif    
    
    gw_header_v1    m_header;
    Events          m_events;
    
    char*   m_buffer;
    size_t  m_bufSize;
    
};

template<> void TestSource::CellDataEvent ( ColumnId p_columnId, std :: string p_value )
{
    m_events . push_back ( Event ( evt_cell_data, p_columnId, p_value . size(), p_value . size(), p_value . c_str() ) );
}

#endif