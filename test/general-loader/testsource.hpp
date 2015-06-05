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

#include <kfs/file.h>

#include "../../tools/general-loader/general-writer.h"

#include <string>
#include <vector>
#include <fstream>

class TestSource
{
public:
    typedef uint32_t TableId;
    typedef uint32_t ColumnId;
    
public:
    static bool packed;

public:
    TestSource( const std:: string& p_signature = GW_SIGNATURE, 
                uint32_t p_endness = GW_GOOD_ENDIAN, 
                uint32_t p_version  = GW_CURRENT_VERSION );
    
    ~TestSource();
    
    const std::string & GetDatabaseName() { return m_database; }

    const struct KFile * MakeSource ();
    
    void SchemaEvent ( const std::string& p_schemaFile, const std::string& p_schemaName );
    void DatabaseEvent ( const std::string& p_databaseName );
    void NewTableEvent ( TableId p_id, const std::string& p_table );
    void NewColumnEvent ( ColumnId p_columnId, TableId p_tableId, const std::string& p_column, uint32_t p_elemBits, bool p_compresssed = false );
    void OpenStreamEvent ();
    void CloseStreamEvent ();
    void NextRowEvent ( TableId p_id );
    void MoveAheadEvent ( TableId p_id, uint64_t p_count );
    void CellDefaultEvent ( ColumnId p_columnId, const std :: string& p_value );
    void CellDefaultEvent ( ColumnId p_columnId, uint32_t p_value );
    void CellDefaultEvent ( ColumnId p_columnId, bool p_value );
    void CellEmptyDefaultEvent ( ColumnId p_columnId );
    void ErrorMessageEvent ( const std :: string& p_msg );
    
    template < typename T > void CellDataEvent ( ColumnId p_columnId, T p_value )
    {
        m_buffer -> Write ( Event ( evt_cell_data, p_columnId, 1, sizeof p_value, (const void*)&p_value ) );
    }   // see below for specialization for std::string (follows the class definition)
   
    // use this to capture stream contents to be used in cmdline tests
    void SaveBuffer ( const char* p_filename ) const;
    
    void CellDataEventRaw ( ColumnId p_columnId, size_t p_elemCount, const void* p_value, size_t p_size )
    {
        m_buffer -> Write ( Event ( evt_cell_data, p_columnId, p_elemCount, p_size, p_value ) );
    }   

private:    
    struct Event 
    {
        Event ( gw_evt_id p_event );
        Event ( gw_evt_id p_event, uint32_t p_id1 );
        Event ( gw_evt_id p_event, uint32_t p_id1, uint64_t p_uint64 );
        Event ( gw_evt_id p_event, uint32_t p_id1, uint32_t p_id2, const std::string& p_str, uint32_t p_uint32, uint8_t p_uint8 );
        Event ( gw_evt_id p_event, uint32_t p_id1, const std::string& p_str1 );
        Event ( gw_evt_id p_event, const std::string& p_str1 );
        Event ( gw_evt_id p_event, const std::string& p_str1, const std::string& p_str2 );
        Event ( gw_evt_id p_event, uint32_t p_id1, uint32_t p_elem_count, uint32_t p_val_bytes, const void* p_val );
        
        ~Event();
        
        gw_evt_id               m_event;
        uint32_t                m_id1;
        uint32_t                m_id2;
        
        uint8_t                 m_uint8;
        uint32_t                m_uint32;
        uint64_t                m_uint64;
        
        std :: string           m_str1;
        std :: string           m_str2;
        std :: vector < char >  m_val;
    };
    
    class Buffer
    {
    public:
        Buffer(const std :: string& p_signature, uint32_t p_endness, uint32_t p_version, bool p_packing);
        ~Buffer();

        void Write ( const Event& );
        
        void Save ( std::ostream& p_out ) const;
        
        const char* GetData()   { return m_buffer; }
        size_t GetSize() const  { return m_bufSize; }
        
    private:    
        void WritePacked ( const Event& );
        void WriteUnpacked ( const Event& );
        
        void Write( const void * p_data, size_t p_size );
        void Pad ( size_t p_alignment = 4 );
        
        char*   m_buffer;
        size_t  m_bufSize;
        
        gw_header_v1 m_header;
    };

    std :: string m_database;
    
    Buffer*         m_buffer;
};

template<> void TestSource::CellDataEvent ( ColumnId p_columnId, std :: string p_value );

#endif