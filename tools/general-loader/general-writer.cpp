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

#include "general-writer.hpp"

#include <iterator>
#include <cstdlib>
#include <iomanip>
#include <unistd.h>

#include <assert.h>
#include <string.h>

#define PROGRESS_EVENT 0

namespace ncbi
{

    int GeneralWriter :: addTable ( const std :: string &table_name )
    {        
        if ( isClosed )
            throw "Cannot add tables after end-stream";

        if ( isOpen )
            throw "Cannot add tables after open";
        
        // prediction this is the index
        int id = ( int ) table_names.size() + 1;

        // make sure we never record a table name twice
        std :: pair < std :: map < std :: string, int > :: iterator, bool > result = 
            table_name_idx.insert ( std :: pair < std :: string, int > ( table_name, id ) );
        
        // if first time
        if ( result.second )
        {
            table_names.push_back ( table_name );

            
            table_hdr hdr ( id, evt_new_table );
            hdr.table_name_size = table_name.size();

            // write header        
            write_event ( &hdr, sizeof hdr );

            // write out string data - NOT NUL TERMINATED!!
            internal_write ( table_name.data(), hdr.table_name_size );

            // force a NUL termination by writing 1..4 NUL bytes
            static char zeros [ 4 ];
            internal_write ( zeros, 4 - hdr.table_name_size % 4 );
        }
        
        // 1 based table id
        return result.first->second;
    }
    
    int GeneralWriter :: addColumn ( int table_id,
                             const std :: string &column_name )
    {
        if ( isClosed )
            throw "Cannot add columns after end-stream";

        if ( isOpen )
            throw "Cannot add columns after open";
        
        if ( table_id <= 0 || ( size_t ) table_id > table_names.size () )
            throw "Invalid table id";
        
        // the thing to insert into map
        int_stream stream ( table_id, column_name );

        // prediction this is the index
        int id = ( int ) streams.size() + 1;

        // make sure we never record a column-spec twice
        std :: pair < std :: map < int_stream, int > :: iterator, bool > result =
            column_name_idx.insert ( std :: pair < int_stream, int > ( stream, id ) );
        
        // if first time
        if ( result.second )
        {
            streams.push_back ( stream );

            // TBD - write new column stream event to stream
            column_hdr hdr ( id, evt_new_column );
            hdr.table_id = table_id;
            hdr.column_name_size = column_name.size ();

            // write header        
            write_event ( &hdr, sizeof hdr );

            // write out string data - NOT NUL TERMINATED!!
            internal_write ( column_name.data (), hdr.column_name_size );

            // force a NUL termination by writing 1..4 NUL bytes
            static char zeros [ 4 ];
            internal_write ( zeros, 4 - hdr.column_name_size % 4 );
        }
        
        // 1 based stream id
        return result.first->second;
    }
 
    
    void GeneralWriter :: open ()
    {
        if ( isClosed )
            throw "Cannot open after end-stream";

        if ( ! isOpen )
        {
            if ( table_names.size () == 0 )
                throw "No tables have been added";
            if ( streams.size () == 0 )
                throw "No columns have been added";

            isOpen = true;        

            evt_hdr hdr ( 0, evt_open_stream );

            // write header        
            write_event ( &hdr, sizeof hdr );
        }
    }

    
    void GeneralWriter :: columnDefault ( int stream_id, uint32_t elem_bits, const void *data, uint32_t elem_count )
    {
        if ( stream_id < 0 )
            throw "Stream_id is not valid";
        if ( stream_id > ( int ) streams.size () )
            throw "Stream_id is out of bounds";
        
        if ( isClosed )
            throw "Cannot set column defaults after end-stream";

        if ( ! isOpen )
            throw "Cannot set column defaults before open";

        if ( elem_bits == 0 || elem_count == 0 )
            return;
        
        if ( data == 0 )
            throw "Invalid data ptr";
        
        cell_hdr chunk ( stream_id, evt_cell_default );
        
        assert ( stream_id <= 0x00FFFFFF );

        chunk.elem_bits = elem_bits;
        chunk.elem_count = elem_count;

        size_t num_bytes = ( size_t ) ( ( uint64_t ) elem_bits * elem_count + 7 ) / 8;
        
        write_event ( &chunk, sizeof chunk );
        internal_write ( ( const char * ) data, num_bytes );
        
        if ( num_bytes % 4 != 0 )
        {
            static char zeros [ 4 ];
            internal_write ( zeros, 4 - num_bytes % 4 );
        }
    }

    void GeneralWriter :: write ( int stream_id, uint32_t elem_bits, const void *data, uint32_t elem_count )
    {
        if ( stream_id < 0 )
            throw "Stream_id is not valid";
        if ( stream_id > ( int ) streams.size () )
            throw "Stream_id is out of bounds";
        
        if ( isClosed )
            throw "Cannot write column data after end-stream";

        if ( ! isOpen )
            throw "Cannot write column data before open";
        
        if ( elem_bits == 0 || elem_count == 0 )
            return;
        
        if ( data == 0 )
            throw "Invalid data ptr";
        
        cell_hdr chunk ( stream_id, evt_cell_data );
        
        assert ( stream_id <= 0x00FFFFFF );

        chunk.elem_bits = elem_bits;
        chunk.elem_count = elem_count;

        size_t num_bytes = ( size_t ) ( ( uint64_t ) elem_bits * elem_count + 7 ) / 8;
        
        write_event ( &chunk, sizeof chunk );
        internal_write ( ( const char * ) data, num_bytes );
        
        if ( num_bytes % 4 != 0 )
        {
            static char zeros [ 4 ];
            internal_write ( zeros, 4 - num_bytes % 4 );
        }
    }

    void GeneralWriter :: nextRow ( int table_id )
    {
        if ( isClosed )
            throw "Cannot commit table after end-stream";

        if ( ! isOpen )
            throw "Cannot commit table before open";
        
        if ( table_id < 0 || ( size_t ) table_id > table_names.size () )
            throw "Invalid table id";

        evt_hdr hdr ( table_id, evt_next_row );

        assert ( table_id <= 0x00FFFFFF );

        write_event ( &hdr, sizeof hdr );
    }

    void GeneralWriter :: logError ( const std :: string & msg )
    {
        if ( isClosed )
        {
            errmsg_hdr hdr ( 0, evt_errmsg );

            hdr . msg_size = msg . size ();

            // write header        
            write_event ( &hdr, sizeof hdr );

            // write out string data - NOT NUL TERMINATED!!
            internal_write ( msg.data (), hdr.msg_size );

            // force a NUL termination by writing 1..4 NUL bytes
            static char zeros [ 4 ];
            internal_write ( zeros, 4 - hdr.msg_size % 4 );
        }
    }

    void GeneralWriter :: endStream ()
    {
        if ( ! isClosed )
        {
            evt_hdr hdr ( 0, evt_end_stream );

            write_event ( &hdr, sizeof hdr );

            isClosed = true;
        }
    }

    
    // Constructors
    GeneralWriter :: GeneralWriter ( const std :: string &out_path,
                             const std :: string &remote_db,
                             const std :: string &schema_file_name, 
                             const std :: string &schema_db_spec )
    : out ( out_path.c_str(), std::ofstream::binary )
    , remote_db ( remote_db )
    , schema_file_name ( schema_file_name )
    , schema_db_spec ( schema_db_spec )
    , evt_count ( 0 )
    , byte_count ( 0 )
    , out_fd ( -1 )
    , isOpen ( false )
    , isClosed ( false )
    {
        writeHeader ();
    }

    
    // Constructors
    GeneralWriter :: GeneralWriter ( int _out_fd,
                             const std :: string &remote_db,
                             const std :: string &schema_file_name, 
                             const std :: string &schema_db_spec )
    : remote_db ( remote_db )
    , schema_file_name ( schema_file_name )
    , schema_db_spec ( schema_db_spec )
    , evt_count ( 0 )
    , byte_count ( 0 )
    , out_fd ( _out_fd )
    , isOpen ( false )
    , isClosed ( false )
    {
        writeHeader ();
    }
    
    GeneralWriter :: ~GeneralWriter ()
    {
        endStream ();
        if ( out_fd < 0 )
            out.flush ();
    }

    bool GeneralWriter :: int_stream :: operator < ( const int_stream &s ) const
    {
        if ( table_id != s.table_id )
            return table_id < s.table_id;
        return column_name.compare ( s.column_name ) < 0;
    }
    
    GeneralWriter :: int_stream :: int_stream ( int _table_id, const std :: string &_column_name )
    : table_id ( _table_id )
    , column_name ( _column_name )
    {
        
    }

    // Private methods

    void GeneralWriter :: writeHeader ()
    {
        header hdr;
        memcpy ( hdr.signature, GW_SIGNATURE, sizeof hdr.signature );
        hdr.endian = GW_GOOD_ENDIAN;
        hdr.version = GW_CURRENT_VERSION;
        hdr.remote_db_name_size = remote_db.size ();
        hdr.schema_file_name_size = schema_file_name.size ();
        hdr.schema_db_spec_size = schema_db_spec.size ();

        internal_write ( &hdr, sizeof hdr );
        internal_write ( remote_db.c_str (), hdr.remote_db_name_size + 1 );
        internal_write ( schema_file_name.c_str (), hdr.schema_file_name_size + 1 );
        internal_write ( schema_db_spec.c_str (), hdr.schema_db_spec_size + 1 );

        size_t num_bytes =  hdr.remote_db_name_size + hdr.schema_file_name_size + hdr.schema_db_spec_size + 3;

        if ( num_bytes % 4 != 0 )
        {
            static char zeros [ 4 ];
            internal_write ( zeros, 4 - num_bytes % 4 );
        }

    }

    void GeneralWriter :: internal_write ( const void * data, size_t num_bytes )
    {
        if ( out_fd < 0 )
        {
            out.write ( ( const char * ) data, num_bytes );
            byte_count += num_bytes;
        }
        else
        {
            size_t total;
            const uint8_t * p = ( const uint8_t * ) data;
            for ( total = 0; total < num_bytes; )
            {
                ssize_t num_writ = :: write ( out_fd, & p [ total ], num_bytes - total );
                if ( num_writ < 0 )
                    throw "Error writing to fd";
                if ( num_writ == 0 )
                    throw "Transfer incomplete writing to fd";
                total += num_writ;
            }
            byte_count += total;
        }
    }

    void GeneralWriter :: write_event ( const evt_hdr * evt, size_t evt_size )
    {
#if PROGRESS_EVENT
        uint64_t ec = evt_count;
        if ( ( ec % 10000 ) == 0 )
        {
            if ( ( ec % 500000 ) == 0 )
                std :: cerr << "\n%  [" << std :: setw ( 12 ) << byte_count << "] " << std :: setw ( 9 ) << ec + 1 << ' ';
            std :: cerr << '.';
        }
#endif
        ++ evt_count;

        internal_write ( evt, evt_size );
    }

}
