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
struct VDBManager;
struct VSchema;

#define GeneralLoaderSignatureString GW_SIGNATURE

class GeneralLoader
{
public:
    static const uint32_t MaxPackedString = 256;
    
public:
    GeneralLoader ( const std :: string& p_programName, const struct KStream& p_input );
    ~GeneralLoader ();
    
    void AddSchemaIncludePath( const std::string& p_path );
    void AddSchemaFile( const std::string& p_file );
    void SetTargetOverride( const std::string& p_path );
    
    rc_t Run ();
    
private:

    typedef std::vector < std::string > Paths;

private:    

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
    
    class DatabaseLoader
    {
    public:
        struct Table
        {
            std :: string name;
            uint32_t databaseId; // key into Databases
            uint32_t cursorIdx;   
        };
        
        struct Column
        {
            std :: string name;
            
            uint32_t tableId;       // key into Tables
            uint32_t cursorIdx;     // index into Cursors
            uint32_t columnIdx;     // index in the VCursor
            uint32_t elemBits;
            uint32_t flagBits;
            
            typedef std :: map < std :: string, std :: string > Metadata;
            Metadata metadata;
            
            bool IsCompressed () const { return ( flagBits & 1 ) == 1; }
        };

    public:
        DatabaseLoader ( const std :: string& p_programName, const Paths& p_includePaths, const Paths& p_schemas, const std::string& p_dbNameOverride = std::string() );
        ~DatabaseLoader();
    
        rc_t UseSchema ( const std :: string& p_file, const std :: string& p_name );
        rc_t RemotePath ( const std :: string& p_path );
        rc_t SoftwareName ( const std :: string& p_softwareName, const std :: string& p_version );
        rc_t NewTable ( uint32_t p_tableId, const std :: string& p_tableName );
        rc_t NewColumn ( uint32_t p_columnId, 
                         uint32_t p_tableId, 
                         uint32_t p_elemBits, 
                         uint8_t p_flags, 
                         const std :: string& p_columnName );
                         
        rc_t DBMetadataNode ( uint32_t p_objId, const std :: string& p_metadata_node, const std :: string& p_value );
        rc_t TblMetadataNode ( uint32_t p_objId, const std :: string& p_metadata_node, const std :: string& p_value );
        rc_t ColMetadataNode ( uint32_t p_objId, const std :: string& p_metadata_node, const std :: string& p_value );
        
        rc_t AddMbrDB ( uint32_t p_objId, uint32_t p_parentId, const std :: string &mbr_name, const std :: string &db_name, uint8_t p_create_mode );
        rc_t AddMbrTbl ( uint32_t p_objId, uint32_t p_parentId, const std :: string &mbr_name, const std :: string &db_name, uint8_t p_create_mode );
        
        rc_t CellData    ( uint32_t p_columnId, const void* p_data, size_t p_elemCount );
        rc_t CellDefault ( uint32_t p_columnId, const void* p_data, size_t p_elemCount );
        rc_t NextRow ( uint32_t p_tableId );
        rc_t MoveAhead ( uint32_t p_tableId, uint64_t p_count );
        rc_t ErrorMessage ( const std :: string& p_text );
        rc_t LogMessage ( const std :: string& p_text );
        rc_t ProgressMessage ( const std :: string& p_name, uint32_t p_pid, uint32_t p_timestamp, uint32_t p_version, uint32_t p_percent );
        rc_t OpenStream ();
        rc_t CloseStream ();
        
        const std :: string& GetDatabaseName() const { return m_databaseName; }
        const Column* GetColumn ( uint32_t p_columnId ) const; 
        
    private:
        // Active Cursors
        typedef std::vector < struct VCursor * > Cursors;
        
        // from TableId to Table
        typedef std::map < uint32_t, Table > Tables; 
        
        // from ColumnId to Column
        typedef std::map < uint32_t, Column > Columns; 
        
        // From database id to VDatabase. id == 0 for the root database.
        typedef std::map < uint32_t, VDatabase* > Databases; 
        
        // From database id to parent database id 
        typedef std::map < uint32_t, uint32_t > DatabaseToParent; 
        
    private:
        rc_t MakeDatabase ( uint32_t p_id );
        rc_t CursorWrite   ( const Column& p_col, const void* p_data, size_t p_size );
        rc_t CursorDefault ( const Column& p_col, const void* p_data, size_t p_size );
        rc_t SaveColumnMetadata ( const Column& p_col );

    private:
        Paths                   m_includePaths;
        Paths                   m_schemas;
    
        std :: string           m_programName;
        std :: string           m_databaseName;
        std :: string           m_schemaName;
        
        std :: string           m_softwareName;
        ver_t                   m_softwareVersion;
    
        Cursors                 m_cursors;
        Tables                  m_tables;
        Columns                 m_columns;
        Databases               m_databases;    
        DatabaseToParent        m_dbParents;    
        
        struct VDBManager*      m_mgr;
        struct VSchema*         m_schema;
        
        bool                    m_databaseNameOverridden;
    };

    class ProtocolParser
    {
    public:
        virtual rc_t ParseEvents ( Reader&, DatabaseLoader& ) = 0;
        
    protected:
        template <typename TEvent> rc_t ReadEvent ( Reader& p_reader, TEvent& p_event );
    };
    
    class UnpackedProtocolParser : public ProtocolParser
    {
    public:
        virtual rc_t ParseEvents ( Reader&, DatabaseLoader& );
    };
    
    class PackedProtocolParser : public ProtocolParser
    {
    public:
        virtual rc_t ParseEvents ( Reader&, DatabaseLoader& );
        
    private:
        // read p_dataSize bytes and use one of the decoder functions in utf8-like-int-codec.h to unpack a sequence of integer values, 
        // stored in m_unpackingBuf as a collection of bytes
        template < typename T_uintXX > rc_t UncompressInt ( Reader& p_reader, uint32_t p_dataSize, int ( * p_decode ) ( uint8_t const* buf_start, uint8_t const* buf_xend, T_uintXX* ret_decoded ) );
        
        rc_t ParseData ( Reader& p_reader, DatabaseLoader& p_dbLoader, uint32_t p_columnId, uint32_t p_dataSize );
        
        std::vector<uint8_t>    m_unpackingBuf;
    };
    
private:    
    GeneralLoader(const GeneralLoader&);
    GeneralLoader& operator = ( const GeneralLoader&);
    
    rc_t ReadHeader ( bool& p_packed );
    
    void CleanUp ();
    
    static void SplitAndAdd( Paths& p_paths, const std::string& p_path );
    
private:    
    std::string             m_programName;
    Reader                  m_reader;
    Paths                   m_includePaths;
    Paths                   m_schemas;
    std::string             m_targetOverride;
};

#endif
