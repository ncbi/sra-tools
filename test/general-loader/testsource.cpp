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

#include "testsource.hpp"

#include <kfs/ramfile.h>

#include <sysalloc.h>

#include <cstring>

#include <stdexcept> 

using namespace std;
using namespace ncbi;

/////////////////////////////////////////////// TestSource::Buffer

TestSource::Buffer::Buffer ( const string& p_signature, uint32_t p_endness, uint32_t p_version, bool p_packing )
:   m_buffer ( 0 ), m_bufSize ( 0 )
{
    init ( m_header );
    strncpy ( m_header . dad . signature, p_signature . c_str(), sizeof m_header . dad . signature );
    m_header . dad . endian   = p_endness;
    m_header . dad . version  = p_version;
    m_header . packing = p_packing ? 1 : 0;
    
    Write ( & m_header, m_header . dad . hdr_size ); 
}
    
TestSource::Buffer::~Buffer ()
{
    free ( m_buffer );
}

void 
TestSource::Buffer::Save ( std::ostream& p_out ) const 
{ 
    p_out . write ( m_buffer, m_bufSize ); 
}
    
void 
TestSource::Buffer::Write ( const void * p_data, size_t p_size )
{
    m_buffer = ( char * ) realloc ( m_buffer, m_bufSize + p_size );
    if ( m_buffer == 0 )
        throw logic_error ( "TestSource::Buffer::Write: realloc failed" );
        
    memcpy ( m_buffer + m_bufSize, p_data, p_size );
    m_bufSize += p_size;
}

void 
TestSource::Buffer::Pad ( size_t p_alignment )
{
    size_t newSize = ( m_bufSize + p_alignment - 1 ) / p_alignment;
    newSize *= p_alignment;
    if ( m_bufSize != newSize ) 
    {
        m_buffer = ( char * ) realloc ( m_buffer, newSize );
        if ( m_buffer == 0 )
            throw logic_error ( "TestSource::Buffer::Write: realloc failed" );
        memset ( m_buffer + m_bufSize, 0, newSize - m_bufSize );
        m_bufSize = newSize;
    }
}

void  
TestSource::Buffer::Write ( const TestSource::Event& p_event )
{
    if ( m_header . packing )
    {
        WritePacked ( p_event );
    }
    else
    {
        WriteUnpacked ( p_event );
    }
}

void    
TestSource::Buffer::WriteUnpacked ( const TestSource::Event& p_event )
{
    Pad ();
    switch ( p_event . m_event )
    {
    case evt_use_schema:
        {   
            gw_2string_evt_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            set_size1 ( hdr, p_event . m_str1 . size() );
            set_size2 ( hdr, p_event . m_str2 . size() );
            
            Write ( & hdr, sizeof hdr ); 
            Write ( p_event . m_str1 . c_str(), p_event . m_str1 . size() ); 
            Write ( p_event . m_str2 . c_str(), p_event . m_str2 . size() ); 
        }
        break;
        
    case evt_remote_path:
    case evt_new_table:
    case evt_errmsg:
        {   
            gw_1string_evt_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            set_size ( hdr, p_event . m_str1 . size () );
            
            Write ( & hdr, sizeof hdr ); 
            Write ( p_event . m_str1 . c_str(), p_event . m_str1 . size() ); 
        }
        break;
        
    case evt_new_column :
        {
            gw_column_evt_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            set_table_id ( hdr, p_event . m_id2 );
            set_elem_bits ( hdr, p_event . m_uint );
            set_name_size ( hdr, p_event . m_str1 . size () );
            
            Write ( & hdr, sizeof hdr );
            Write ( p_event . m_str1 . c_str (), p_event . m_str1 . size () );
        }
        break;
        
    case evt_open_stream:
    case evt_end_stream:
        {
            gw_evt_hdr_v1 hdr;
            init ( hdr, 0, p_event . m_event );
            Write ( & hdr, sizeof hdr );
        }
        break;

    case evt_cell_data :
    case evt_cell_default :
        {
            gw_data_evt_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            set_elem_count ( hdr, p_event . m_uint );
            
            Write ( & hdr, sizeof hdr );
            Write ( p_event . m_val . data(), p_event . m_val . size() );
        }
        break;
        
    case evt_next_row:
        {
            gw_evt_hdr_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            Write ( & hdr, sizeof hdr );
        }
        break;
        
    case evt_move_ahead:
        {
            gw_move_ahead_evt_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            set_nrows ( hdr, p_event . m_uint64 );
            Write ( & hdr, sizeof hdr );
        }
        break;
        
    default:
        throw logic_error ( "TestSource::Buffer::WriteUnpacked: event not implemented" );
    }
}
    
void    
TestSource::Buffer::WritePacked ( const TestSource::Event& p_event )
{
    Pad ();
    switch ( p_event . m_event )
    {
    case evt_use_schema:
        {   
            gwp_2string_evt_U16_v1 hdr;
            init ( hdr, p_event . m_id1, evt_use_schema2 );
        
            set_size1 ( hdr, p_event . m_str1 . size() );
            set_size2 ( hdr, p_event . m_str2 . size() );

            Write ( & hdr, sizeof hdr ); 
            Write ( p_event . m_str1 . c_str(), p_event . m_str1 . size() ); 
            Write ( p_event . m_str2 . c_str(), p_event . m_str2 . size() ); 
        }
        break;
        
    //TODO: the following 3 cases are almost identical - refactor
    case evt_remote_path:
        {   
            gwp_1string_evt_U16_v1 hdr;
            init ( hdr, p_event . m_id1, evt_remote_path2 );
            set_size ( hdr, p_event . m_str1 . size () );
            
            Write ( & hdr, sizeof hdr ); 
            Write ( p_event . m_str1 . c_str(), p_event . m_str1 . size() ); 
        }
        break;
    case evt_new_table:
        {   
            gwp_1string_evt_U16_v1 hdr;
            init ( hdr, p_event . m_id1, evt_new_table2 );
            set_size ( hdr, p_event . m_str1 . size () );
            
            Write ( & hdr, sizeof hdr ); 
            Write ( p_event . m_str1 . c_str(), p_event . m_str1 . size() ); 
        }
        break;
    case evt_errmsg:
        {   
            gwp_1string_evt_U16_v1 hdr;
            init ( hdr, p_event . m_id1, evt_errmsg2 );
            set_size ( hdr, p_event . m_str1 . size () );
            
            Write ( & hdr, sizeof hdr ); 
            Write ( p_event . m_str1 . c_str(), p_event . m_str1 . size() ); 
        }
        break;

        
        
    case evt_new_column :
        {
            gwp_column_evt_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            set_table_id ( hdr, p_event . m_id2 );
            set_elem_bits ( hdr, p_event . m_uint );
            set_name_size ( hdr, p_event . m_str1 . size () );
            
            Write ( & hdr, sizeof hdr );
            Write ( p_event . m_str1 . c_str (), p_event . m_str1 . size () );
        }
        break;
        
    case evt_open_stream:
    case evt_end_stream:
        {
            gwp_evt_hdr_v1 hdr;
            init ( hdr, 0, p_event . m_event );
            Write ( & hdr, sizeof hdr );
        }
        break;

    case evt_cell_data :
        {
            gwp_data_evt_U16_v1 hdr;
            init ( hdr, p_event . m_id1, evt_cell_data2 );
            // in the packed message, we specify the number of bytes in the cell
            set_size ( hdr, p_event . m_val . size() ); 
            
            Write ( & hdr, sizeof hdr );
            Write ( p_event . m_val . data(), p_event . m_val . size() );
        }
        break;
    case evt_cell_default :
        {
            gwp_data_evt_U16_v1 hdr;
            init ( hdr, p_event . m_id1, evt_cell_default2 ); //TODO: this is the only difference from evt_cell_data - refactor
            // in the packed message, we specify the number of bytes in the cell
            set_size ( hdr, p_event . m_val . size() ); 
            
            Write ( & hdr, sizeof hdr );
            Write ( p_event . m_val . data(), p_event . m_val . size() );
        }
        break;
        
    case evt_next_row:
        {
            gwp_evt_hdr_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            Write ( & hdr, sizeof hdr );
        }
        break;
        
    case evt_move_ahead:
        {
            gwp_move_ahead_evt_v1 hdr;
            init ( hdr, p_event . m_id1, p_event . m_event );
            set_nrows ( hdr, p_event . m_uint64 );
            Write ( & hdr, sizeof hdr );
        }
        break;
        
    default:
        throw logic_error ( "TestSource::Buffer::WritePacked: event not implemented" );
    }
}

/////////////////////////////////////// TestSource

bool TestSource::packed = false;

TestSource::TestSource ( const string& p_signature, uint32_t p_endness, uint32_t p_version )
:   m_buffer ( new Buffer ( p_signature, p_endness, p_version, packed ) )
{
}

TestSource::~TestSource()
{
    delete m_buffer;
}

const struct KFile * 
TestSource::MakeSource ()
{
    const struct KFile * ret;
    if ( KRamFileMakeRead ( & ret, 
                            const_cast < char* > ( m_buffer -> GetData() ), 
                            m_buffer -> GetSize() ) != 0 
         || ret == 0 )
        throw logic_error ( "TestSource::MakeSource KRamFileMakeRead failed" );
        
    return ret;
}

void 
TestSource::SchemaEvent ( const std::string& p_schemaFile, const std::string& p_schemaName )
{
    m_buffer -> Write ( Event ( evt_use_schema, p_schemaFile, p_schemaName ) );
}

void 
TestSource::DatabaseEvent ( const std::string& p_databaseName )
{
    m_buffer -> Write ( Event ( evt_remote_path, p_databaseName ) );
    m_database = p_databaseName;
}

void 
TestSource::NewTableEvent ( TableId p_id, const std::string& p_table )
{
    m_buffer -> Write ( Event ( evt_new_table, p_id, p_table ) );
}

void 
TestSource::NewColumnEvent ( ColumnId p_columnId, TableId p_tableId, const std::string& p_column, uint32_t p_elemBits )
{
    m_buffer -> Write ( Event ( evt_new_column, p_columnId, p_tableId, p_column, p_elemBits ) );
    //TODO: support integer compaction
}
    
void 
TestSource::OpenStreamEvent ()
{
    m_buffer -> Write ( Event ( evt_open_stream ) );
}

void 
TestSource::CloseStreamEvent ()
{
    m_buffer -> Write ( Event ( evt_end_stream ) );
}

void 
TestSource::NextRowEvent ( TableId p_id )
{
    m_buffer -> Write ( Event ( evt_next_row, p_id ) );
}

void 
TestSource::MoveAheadEvent ( TableId p_id, uint64_t p_count )
{
    m_buffer -> Write ( Event ( evt_move_ahead, p_id, p_count ) );
}

template<> void TestSource::CellDataEvent ( ColumnId p_columnId, string p_value )
{
    m_buffer -> Write ( Event ( evt_cell_data, p_columnId, p_value . size(), p_value . size(), p_value . c_str() ) );
}

void 
TestSource::CellDefaultEvent ( ColumnId p_columnId, const string& p_value )
{
    m_buffer -> Write ( Event ( evt_cell_default, p_columnId, p_value . size(), p_value . size(), p_value . c_str() ) );
}

void 
TestSource::CellDefaultEvent ( ColumnId p_columnId, uint32_t p_value )
{
    m_buffer -> Write ( Event ( evt_cell_default, p_columnId, 1,  sizeof p_value, (const void*)&p_value ) );
}
void 
TestSource::CellDefaultEvent ( ColumnId p_columnId, bool p_value )
{
    m_buffer -> Write ( Event ( evt_cell_default, p_columnId, 1, sizeof p_value, (const void*)&p_value ) );
}

void 
TestSource::ErrorMessageEvent ( const string& p_msg )
{
    m_buffer -> Write ( Event ( evt_errmsg, p_msg ) );
}

void 
TestSource::SaveBuffer ( const char* p_filename ) const
{
    ofstream out ( p_filename );
    m_buffer -> Save ( out );
}

TestSource::Event::Event ( gw_evt_id p_event )
:   m_event ( p_event ),
    m_id1 ( 0 ),
    m_id2 ( 0 ),
    m_uint ( 0 ),
    m_uint64 ( 0 )
{
}
        
TestSource::Event::Event ( gw_evt_id p_event, uint32_t p_id1 )
:   m_event ( p_event ),
    m_id1 ( p_id1 ),
    m_id2 ( 0 ),
    m_uint ( 0 ),
    m_uint64 ( 0 )
{
}

TestSource::Event::Event ( gw_evt_id p_event, uint32_t p_id1, uint64_t p_uint64 )
:   m_event ( p_event ),
    m_id1 ( p_id1 ),
    m_id2 ( 0 ),
    m_uint ( 0 ),
    m_uint64 ( p_uint64 )
{
}

TestSource::Event::Event ( gw_evt_id p_event, uint32_t p_id1, uint32_t p_id2, const std::string& p_str, uint32_t p_uint1 )
:   m_event ( p_event ),
    m_id1 ( p_id1 ),
    m_id2 ( p_id2 ),
    m_uint ( p_uint1 ),
    m_uint64 ( 0 ),
    m_str1 ( p_str )
{
}

TestSource::Event::Event ( gw_evt_id p_event, uint32_t p_id, const std::string& p_str1 )
:   m_event ( p_event ),
    m_id1 ( p_id ),
    m_id2 ( 0 ),
    m_uint ( 0 ),
    m_uint64 ( 0 ),
    m_str1 ( p_str1 )
{
}

TestSource::Event::Event ( gw_evt_id p_event, const std::string& p_str1 )
:   m_event ( p_event ),
    m_id1 ( 0 ),
    m_id2 ( 0 ),
    m_uint ( 0 ),
    m_uint64 ( 0 ),
    m_str1 ( p_str1 )
{
}

TestSource::Event::Event ( gw_evt_id p_event, const std::string& p_str1, const std::string& p_str2 )
:   m_event ( p_event ),
    m_id1 ( 0 ),
    m_id2 ( 0 ),
    m_uint ( 0 ),
    m_uint64 ( 0 ),
    m_str1 ( p_str1 ),
    m_str2 ( p_str2 )
{
}

TestSource::Event::Event ( gw_evt_id p_event, uint32_t p_id1, uint32_t p_elem_count, uint32_t p_val_bytes, const void* p_val )
:   m_event ( p_event ),
    m_id1 ( p_id1 ),
    m_id2 ( 0 ),
    m_uint ( p_elem_count ),
    m_uint64 ( 0 )
{
    const char* v = ( const char* ) p_val;
    for ( size_t i = 0; i != p_val_bytes; ++i )
    {
        m_val . push_back ( v [ i ] );
    }
}

TestSource::Event::~Event()
{
}
