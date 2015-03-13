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

#ifndef _sra_tools_hpp_general_loader_
#define _sra_tools_hpp_general_loader_

#include <klib/defs.h>

#include <string>
#include <vector>
#include <map>

struct KStream;
struct VCursor;
struct VDatabase;

#define GeneralLoaderSignatureString "NCBIgnld"

class GeneralLoader
{
public:
    
    struct Header
    {
        char        signature[8];   // 8 characters to identify file type                    
        uint32_t    endian;         // an internally known pattern to identify endian                  
        uint32_t    version;        // a single-integer version number                     
        uint32_t    remote_db_name_size;
        uint32_t    schema_file_name_size;
        uint32_t    schema_spec_size;
        // string data follows: 3 strings plus 1 NUL byte for each, 4-byte aligned
        // uint32_t data [ ( ( remote_db_name_size + schema_file_name_size + schema_spec_size + 3 ) + 3 ) / 4 ];
    };
    
    enum Evt_id
    {
        evt_end_stream,
        evt_new_table,
        evt_new_column,
        evt_open_stream,
        evt_cell_default, 
        evt_cell_data, 
        evt_next_row 
    };
    
    struct Table_hdr
    {
        uint32_t id : 24;
        uint32_t evt : 8;
        uint32_t table_name_size;
        // uint32_t data [ ( ( table_name_size + 1 ) + 3 ) / 4 ];
    };

    struct Column_hdr
    {
        uint32_t id : 24;
        uint32_t evt : 8;
        uint32_t table_id;
        uint32_t column_name_size;
        // uint32_t data [ ( ( column_name_size + 1 ) + 3 ) / 4 ];
    };
    
    struct Cell_hdr
    {
        uint32_t id : 24;
        uint32_t evt : 8;
        uint32_t elem_bits;
        uint32_t elem_count;
        // uint32_t data [ ( elem_bits * elem_count + 31 ) / 32 ];
    };
    
    struct Evt_hdr
    {   // used for "open-stream" and "end-of-stream" events
        uint32_t id : 24;
        uint32_t evt : 8;
    };
    
public:
    GeneralLoader ( const struct KStream& p_input );
    ~GeneralLoader ();
    
    void AddSchemaIncludePath( const std::string& p_path );
    
    rc_t Run ();
    
    void Reset();
    
private:
    // Active cursors
    typedef std::vector < struct VCursor * > Cursors;

    // from table id to VCursor
    // value_type : index into Cursors
    typedef std::map < uint32_t, uint32_t > TableIdToCursor; 
    
    // From column id to VCursor.
    // value_type::first    : index into Cursors
    // value_type::second   : colIdx in the VCursor
    typedef std::map < uint32_t, std::pair < uint32_t, uint32_t > > ColumnToCursor; 
    
    typedef std::vector < std::string > Paths;

    rc_t ReadHeader ();
    rc_t ReadMetadata ();
    rc_t MakeDatabase ();
    rc_t MakeCursor ( const char*  p_table );
    rc_t MakeCursors ();
    rc_t OpenCursors ();
    rc_t ReadData ();
    void CleanUp ();
    
    class Reader
    {
    public:
        Reader( const struct KStream& p_input );
        ~Reader();
        
        // read into caller's buffer
        rc_t Read( void * p_buffer, size_t p_size ); 
        
        // if rc == 0, there are p_size bytes available through GetBuffer until the next call to Read
        rc_t Read( size_t p_size ); 
        
        const void* GetBuffer() const { return m_buffer; }
        
        void Align( uint8_t p_bytes = 4 );
        
    private:
        const struct KStream& m_input;
        void* m_buffer;
        size_t m_bufSize;
        size_t m_readCount;
    };
    
    Reader m_reader;
    
    Header  m_header;
    char*   m_headerNames;
    
    Paths               m_includePaths;
    struct VDatabase*   m_db;
    Cursors             m_cursors;
    TableIdToCursor     m_tables;
    ColumnToCursor      m_columns;
};

#endif
