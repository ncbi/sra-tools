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
#if GW_CURRENT_VERSION >= 2

    // ask the general-loader to use this when naming its output
    void GeneralWriter :: setRemotePath ( const std :: string & remote_db )
    {
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case header_written:
            new_state = remote_name_sent;
            break;
        case schema_sent:
            new_state = remote_name_and_schema_sent;
            break;
        default:
            throw "state violation setting remote path";
        }

        size_t str_size = remote_db . size ();
        if ( str_size > 0x10000 )
            throw "remote path too long";

        gw_string_hdr_U16_v2 hdr ( 0, evt_remote_path );
        hdr . string_size = ( uint16_t ) ( str_size  - 1 );
        write_event ( &hdr, sizeof hdr );
        internal_write ( remote_db . c_str (), str_size + 1 );

        state = new_state;
    }

    // tell the general-loader to use this pre-defined schema
    void GeneralWriter :: useSchema ( const std :: string & schema_file_name,
                                      const std :: string & schema_db_spec )
    {
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case header_written:
            new_state = schema_sent;
            break;
        case remote_name_sent:
            new_state = remote_name_and_schema_sent;
            break;
        default:
            throw "state violation using schema";
        }

        size_t str1_size = schema_file_name . size ();
        if ( str1_size > 0x10000 )
            throw "schema path too long";

        size_t str2_size = schema_db_spec . size ();
        if ( str2_size > 0x10000 )
            throw "schema spec too long";

        gw_string2_hdr_U16_v2 hdr ( 0, evt_use_schema );
        hdr . string1_size = ( uint16_t ) ( str1_size - 1 );
        hdr . string2_size = ( uint16_t ) ( str2_size - 1 );
        write_event ( &hdr, sizeof hdr );
        internal_write ( schema_file_name . c_str (), str1_size + 1 );
        internal_write ( schema_db_spec . c_str (), str2_size + 1 );

        state = new_state;
    }
#endif

    int GeneralWriter :: addTable ( const std :: string &table_name )
    {        
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case schema_sent:
        case remote_name_and_schema_sent:
            new_state = have_table;
            break;
        case have_table:
        case have_column:
            new_state = state;
            break;
        default:
            throw "state violation adding table";
        }
        
        // prediction this is the index
        int id = ( int ) table_names.size() + 1;
#if GW_CURRENT_VERSION >= 2
        if ( id > 256 )
            throw "maximum number of tables exceeded";
#endif

        // make sure we never record a table name twice
        std :: pair < std :: map < std :: string, int > :: iterator, bool > result = 
            table_name_idx.insert ( std :: pair < std :: string, int > ( table_name, id ) );
        
        // if first time
        if ( result.second )
        {
            table_names.push_back ( table_name );

            size_t str_size = table_name . size ();
#if GW_CURRENT_VERSION >= 2
            if ( str_size > 0x10000 )
                throw "maximum table name length exceeded";
#endif
            table_hdr hdr ( id, evt_new_table );
#if GW_CURRENT_VERSION >= 2
            hdr . string_size = ( uint16_t ) ( str_size - 1 );
#else
            hdr.table_name_size = ( uint32_t ) str_size;
#endif

            // write header        
            write_event ( &hdr, sizeof hdr );

#if GW_CURRENT_VERSION >= 2
            internal_write ( table_name.c_str (), str_size + 1 );
#else
            // write out string data - NOT NUL TERMINATED!!
            internal_write ( table_name.data(), hdr.table_name_size );

            // force a NUL termination by writing 1..4 NUL bytes
            static char zeros [ 4 ];
            internal_write ( zeros, 4 - hdr.table_name_size % 4 );
#endif
            state = new_state;
        }
        
        // 1 based table id
        return result.first->second;
    }
    
#if GW_CURRENT_VERSION >= 2
    int GeneralWriter :: addColumn ( int table_id,
        const std :: string &column_name, uint32_t elem_bits, uint8_t flag_bits )
#else
    int GeneralWriter :: addColumn ( int table_id,
        const std :: string &column_name )
#endif
    {
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case have_table:
        case have_column:
            new_state = have_column;
            break;
        default:
            throw "state violation adding column";
        }
        
        if ( table_id <= 0 || ( size_t ) table_id > table_names.size () )
            throw "Invalid table id";
        
        // the thing to insert into map
#if GW_CURRENT_VERSION == 1
        int_stream stream ( table_id, column_name );
#else
        int_stream stream ( table_id, column_name, elem_bits, flag_bits );
#endif

        // prediction this is the index
        int id = ( int ) streams.size() + 1;
#if GW_CURRENT_VERSION >= 2
        if ( id > 256 )
            throw "maximum number of columns exceeded";
#endif
        // make sure we never record a column-spec twice
        std :: pair < std :: map < int_stream, int > :: iterator, bool > result =
            column_name_idx.insert ( std :: pair < int_stream, int > ( stream, id ) );
        
        // if first time
        if ( result.second )
        {
            streams.push_back ( stream );

            size_t str_size = column_name . size ();
#if GW_CURRENT_VERSION >= 2
            if ( str_size > 256 )
                throw "maximum column spec length exceeded";
#endif
            // TBD - write new column stream event to stream
            column_hdr hdr ( id, evt_new_column );
#if GW_CURRENT_VERSION == 1
            hdr.table_id = table_id;
            hdr.column_name_size = ( uint32_t ) str_size;
#else
            hdr.table_id = ( uint8_t ) ( table_id - 1 );
            hdr.elem_bits = elem_bits;
            hdr.flag_bits = flag_bits;
            hdr.column_name_size = ( uint8_t ) ( str_size - 1 );
#endif
            // write header        
            write_event ( &hdr, sizeof hdr );

#if GW_CURRENT_VERSION == 1
            // write out string data - NOT NUL TERMINATED!!
            internal_write ( column_name.data (), hdr.column_name_size );

            // force a NUL termination by writing 1..4 NUL bytes
            static char zeros [ 4 ];
            internal_write ( zeros, 4 - hdr.column_name_size % 4 );
#else
            internal_write ( column_name.c_str (), str_size + 1 );
#endif
            state = new_state;
        }
        
        // 1 based stream id
        return result.first->second;
    }
 
    
    void GeneralWriter :: open ()
    {
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case have_column:
            new_state = opened;
            break;
        case opened:
            return;
        default:
            throw "state violation opening stream";
        }

        evt_hdr hdr ( 0, evt_open_stream );

        // write header        
        write_event ( &hdr, sizeof hdr );

        state = new_state;
    }

    
    void GeneralWriter :: columnDefault ( int stream_id, uint32_t elem_bits, const void *data, uint32_t elem_count )
    {
        switch ( state )
        {
        case opened:
            break;
        default:
            throw "state violation setting column default";
        }

        if ( stream_id < 0 )
            throw "Stream_id is not valid";
        if ( stream_id > ( int ) streams.size () )
            throw "Stream_id is out of bounds";

        if ( elem_bits == 0 || elem_count == 0 )
            return;
        
        if ( data == 0 )
            throw "Invalid data ptr";

#if GW_CURRENT_VERSION >= 2
        if ( elem_bits != streams [ stream_id - 1 ] . elem_bits )
            throw "Invalid elem_bits";
#endif

#if GW_CURRENT_VERSION == 1
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
#else
        size_t num_bytes = ( ( size_t ) elem_bits * elem_count + 7 ) / 8;
        if ( num_bytes <= 256 )
        {
            gw_data_hdr_U8_v2 chunk ( stream_id, evt_cell_default );
            chunk . data_size = ( uint8_t ) ( num_bytes - 1 );
            write_event ( &chunk, sizeof chunk );
        }
        else if ( num_bytes <= 0x10000 )
        {
            gw_data_hdr_U16_v2 chunk ( stream_id, evt_cell2_default );
            chunk . data_size = ( uint16_t ) ( num_bytes - 1 );
            write_event ( &chunk, sizeof chunk );
        }
        else
        {
            throw "default cell-data exceeds maximum";
        }
        internal_write ( data, num_bytes );
#endif
    }

    void GeneralWriter :: write ( int stream_id, uint32_t elem_bits, const void *data, uint32_t elem_count )
    {
        switch ( state )
        {
        case opened:
            break;
        default:
            throw "state violation writing column data";
        }

        if ( stream_id < 0 )
            throw "Stream_id is not valid";
        if ( stream_id > ( int ) streams.size () )
            throw "Stream_id is out of bounds";

        if ( elem_bits == 0 || elem_count == 0 )
            return;
        
        if ( data == 0 )
            throw "Invalid data ptr";

#if GW_CURRENT_VERSION >= 2
        if ( elem_bits != streams [ stream_id - 1 ] . elem_bits )
            throw "Invalid elem_bits";
#endif
        
#if GW_CURRENT_VERSION == 1
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
#else
        const uint8_t * dp = ( const uint8_t * ) data;
        size_t num_bytes = ( ( size_t ) elem_bits * elem_count + 7 ) / 8;
        while ( num_bytes >= 0x10000 )
        {
            gw_data_hdr_U16_v2 chunk ( stream_id, evt_cell2_data );
            chunk . data_size = 0xFFFF;
            write_event ( &chunk, sizeof chunk );
            internal_write ( dp, 0x10000 );
            num_bytes -= 0x10000;
            dp += 0x10000;
        }

        if ( num_bytes <= 256 )
        {
            gw_data_hdr_U8_v2 chunk ( stream_id, evt_cell_data );
            chunk . data_size = ( uint8_t ) ( num_bytes - 1 );
            write_event ( &chunk, sizeof chunk );
        }
        else
        {
            gw_data_hdr_U16_v2 chunk ( stream_id, evt_cell2_data );
            chunk . data_size = ( uint16_t ) ( num_bytes - 1 );
            write_event ( &chunk, sizeof chunk );
        }
        internal_write ( data, num_bytes );
#endif
    }

    void GeneralWriter :: nextRow ( int table_id )
    {
        switch ( state )
        {
        case opened:
            break;
        default:
            throw "state violation advancing to next row";
        }

        if ( table_id < 0 || ( size_t ) table_id > table_names.size () )
            throw "Invalid table id";

        evt_hdr hdr ( table_id, evt_next_row );

#if GW_CURRENT_VERSION == 1
        assert ( table_id <= 0x00FFFFFF );
#endif
        write_event ( &hdr, sizeof hdr );
    }

    void GeneralWriter :: logError ( const std :: string & msg )
    {
        switch ( state )
        {
        case header_written:
        case remote_name_sent:
        case schema_sent:
        case remote_name_and_schema_sent:
        case have_table:
        case have_column:
        case opened:
        case error:
            break;
        default:
            return;
        }

        errmsg_hdr hdr ( 0, evt_errmsg );

        size_t str_size = msg . size ();
#if GW_CURRENT_VERSION == 1
        hdr . msg_size = ( uint32_t ) str_size;
#else
        if ( str_size > 256 )
            str_size = 256;
        hdr . string_size = ( uint8_t ) ( str_size - 1 );
#endif

        // write header
        write_event ( &hdr, sizeof hdr );

#if GW_CURRENT_VERSION == 1
        // write out string data - NOT NUL TERMINATED!!
        internal_write ( msg.data (), hdr.msg_size );

        // force a NUL termination by writing 1..4 NUL bytes
        static char zeros [ 4 ];
        internal_write ( zeros, 4 - hdr.msg_size % 4 );
#else
        internal_write ( msg.c_str (), str_size + 1 );
#endif
    }

    void GeneralWriter :: endStream ()
    {
        switch ( state )
        {
        case header_written:
        case remote_name_sent:
        case schema_sent:
        case remote_name_and_schema_sent:
        case have_table:
        case have_column:
        case opened:
        case error:
            break;
        default:
            return;
        }

        evt_hdr hdr ( 0, evt_end_stream );

        write_event ( &hdr, sizeof hdr );

        state = closed;
    }

    
    // Constructors
    GeneralWriter :: GeneralWriter ( const std :: string &out_path,
                             const std :: string &remote_db,
                             const std :: string &schema_file_name, 
                             const std :: string &schema_db_spec )
        : out ( out_path.c_str(), std::ofstream::binary )
#if GW_CURRENT_VERSION == 1
        , remote_db ( remote_db )
        , schema_file_name ( schema_file_name )
        , schema_db_spec ( schema_db_spec )
#endif
        , evt_count ( 0 )
        , byte_count ( 0 )
        , out_fd ( -1 )
        , state ( uninitialized )
    {
        writeHeader ();

#if GW_CURRENT_VERSION >= 2
        setRemotePath ( remote_db );
        useSchema ( schema_file_name, schema_db_spec );
#endif
    }

    
    // Constructors
    GeneralWriter :: GeneralWriter ( int _out_fd,
                             const std :: string &remote_db,
                             const std :: string &schema_file_name, 
                             const std :: string &schema_db_spec )
        :
#if GW_CURRENT_VERSION == 1
          remote_db ( remote_db ),
          schema_file_name ( schema_file_name ),
          schema_db_spec ( schema_db_spec ),
#endif
          evt_count ( 0 )
        , byte_count ( 0 )
        , out_fd ( _out_fd )
        , state ( uninitialized )
    {
        writeHeader ();

#if GW_CURRENT_VERSION >= 2
        setRemotePath ( remote_db );
        useSchema ( schema_file_name, schema_db_spec );
#endif
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
    
#if GW_CURRENT_VERSION == 1
    GeneralWriter :: int_stream :: int_stream ( int _table_id, const std :: string &_column_name )
        : table_id ( _table_id )
        , column_name ( _column_name )
    {
    }
#else
    GeneralWriter :: int_stream :: int_stream ( int _table_id, const std :: string &_column_name, uint32_t _elem_bits, uint8_t _flag_bits )
        : table_id ( _table_id )
        , column_name ( _column_name )
        , elem_bits ( _elem_bits )
        , flag_bits ( _flag_bits )
    {
    }
#endif

    // Private methods

    void GeneralWriter :: writeHeader ()
    {
        header hdr;

#if GW_CURRENT_VERSION == 1
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
        state = remote_name_and_schema_sent;
#else
        internal_write ( &hdr, sizeof hdr );
        state = header_written;
#endif

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

        assert ( evt -> evt () != evt_bad_event );
#if GW_CURRENT_VERSION == 1
        assert ( evt -> evt () <= evt_errmsg );
#else
        assert ( evt -> evt () <= evt_cell2_data );
#endif

        internal_write ( evt, evt_size );
    }

}
