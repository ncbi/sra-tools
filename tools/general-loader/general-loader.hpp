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

#include "general-writer.h"

struct KStream;
struct VCursor;
struct VDatabase;
struct VDBManager;
struct VSchema;

#define GeneralLoaderSignatureString GW_SIGNATURE

class GeneralLoader
{
public:
    typedef struct gw_header_v1 Header;
    typedef enum gw_evt_id Evt_id;
    
public:
    GeneralLoader ( const struct KStream& p_input );
    ~GeneralLoader ();
    
    void AddSchemaIncludePath( const std::string& p_path );
    void AddSchemaFile( const std::string& p_file );
    
    rc_t Run ();
    
    void Reset();
    
private:
    GeneralLoader(const GeneralLoader&);
    GeneralLoader& operator = ( const GeneralLoader&);

    // Active cursors
    typedef std::vector < struct VCursor * > Cursors;

    // from table id to VCursor
    // value_type : index into Cursors
    typedef std::map < uint32_t, uint32_t > TableIdToCursor; 
    
    struct Column
    {
        uint32_t cursorIdx;     // index into Cursors
        uint32_t columnIdx;     // index in the VCursor
        uint32_t elemBits;
        uint32_t flagBits;
    };
    
    // From column id to VCursor.
    // value_type::first    : index into Cursors
    // value_type::second   : colIdx in the VCursor
    typedef std::map < uint32_t, Column > Columns; 
    
    typedef std::vector < std::string > Paths;

    rc_t ReadHeader ();
    rc_t ReadUnpackedEvents ();
    rc_t ReadPackedEvents ();
    
    // read and handle individual events
    rc_t Handle_UseSchema ( const std :: string& p_file, const std :: string& p_name );
    rc_t Handle_RemotePath ( const std :: string& p_path );
    rc_t Handle_NewTable ( uint32_t p_tableId, const std :: string& p_tableName );
    rc_t Handle_NewColumn ( uint32_t p_columnId, 
                            uint32_t p_tableId, 
                            uint32_t p_elemBits, 
                            uint8_t p_flags, 
                            const std :: string& p_columnName );
    rc_t Handle_CellData ( uint32_t p_columnId );
    rc_t Handle_CellData_Packed ( uint32_t p_columnId );
    rc_t Handle_CellDefault ( uint32_t p_columnId );
    rc_t Handle_CellDefault_Packed ( uint32_t p_columnId );
    rc_t HandleNextRow ( uint32_t p_tableId );
    rc_t Handle_MoveAhead ( uint32_t p_tableId, uint64_t p_count );
    rc_t Handle_ErrorMessage ( const std :: string& p_text );
    
    void CleanUp ();
    
    rc_t MakeCursors ();
    rc_t OpenCursors ();
    rc_t CloseCursors ();
    
    static void SplitAndAdd( Paths& p_paths, const std::string& p_path );
    
    
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
        
        uint64_t GetReadCount() { return m_readCount; }
        
    private:
        const struct KStream& m_input;
        void* m_buffer;
        size_t m_bufSize;
        uint64_t m_readCount;
    };
    
    Reader m_reader;
    
    Header          m_header;
    std::string     m_databaseName;
    std::string     m_schemaName;
    
    Paths               m_includePaths;
    Paths               m_schemas;
    struct VDBManager*  m_mgr;
    struct VSchema*     m_schema;
    struct VDatabase*   m_db;
    
    Cursors             m_cursors;
    TableIdToCursor     m_tables;
    Columns             m_columns;
};

#endif
