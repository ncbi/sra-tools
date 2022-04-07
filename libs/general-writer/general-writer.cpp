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
#include "utf8-like-int-codec.h"

#include <iterator>
#include <cstdlib>
#include <iomanip>

#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#define PROGRESS_EVENT 0

namespace ncbi
{

#if GW_CURRENT_VERSION <= 2
    typedef :: gwp_1string_evt_v1 gwp_1string_evt;
    typedef :: gwp_2string_evt_v1 gwp_2string_evt;
    typedef :: gwp_column_evt_v1 gwp_column_evt;
    typedef :: gwp_data_evt_v1 gwp_data_evt;
    typedef :: gwp_1string_evt_U16_v1 gwp_1string_evt_U16;
    typedef :: gwp_2string_evt_U16_v1 gwp_2string_evt_U16;
    typedef :: gwp_data_evt_U16_v1 gwp_data_evt_U16;
#else
#error "unrecognized GW version"
#endif

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
        case software_name_sent:
            new_state = remote_name_and_software_name_sent;
            break;
        case schema_and_software_name_sent:
            new_state = remote_name_schema_and_software_name_sent;
            break;
        default:
            throw "state violation setting remote path";
        }

        size_t str_size = remote_db . size ();
        if ( str_size > 0x10000 )
            throw "remote path too long";

        gwp_1string_evt_U16 hdr;
        init ( hdr, 0, evt_remote_path2 );
        set_size ( hdr, str_size );
        write_event ( & hdr . dad, sizeof hdr );
        internal_write ( remote_db . data (), str_size );

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
        case software_name_sent:
            new_state = schema_and_software_name_sent;
            break;
        case remote_name_and_software_name_sent:
            new_state = remote_name_schema_and_software_name_sent;
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

        gwp_2string_evt_U16 hdr;
        init ( hdr, 0, evt_use_schema2 );
        set_size1 ( hdr, str1_size );
        set_size2 ( hdr, str2_size );
        write_event ( & hdr . dad, sizeof hdr );
        internal_write ( schema_file_name . data (), str1_size );
        internal_write ( schema_db_spec . data (), str2_size );

        state = new_state;
    }

    void GeneralWriter :: setSoftwareName ( const std :: string & name,
                                            const std :: string & version )
    {
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case header_written:
            new_state = software_name_sent;
            break;
        case remote_name_sent:
            new_state = remote_name_and_software_name_sent;
            break;
        case schema_sent:
            new_state = schema_and_software_name_sent;
            break;
        case remote_name_and_schema_sent:
            new_state = remote_name_schema_and_software_name_sent;
            break;
        default:
            throw "state violation using schema";
        }

        size_t str1_size = name . size ();
        if ( str1_size > 0x100 )
            throw "name too long";

        size_t str2_size = version . size ();
        if ( str2_size > 0x100 )
            throw "version too long";

        gwp_2string_evt_v1 hdr;
        init ( hdr, 0, evt_software_name );
        set_size1 ( hdr, str1_size );
        set_size2 ( hdr, str2_size );
        write_event ( & hdr . dad, sizeof hdr );
        internal_write ( name . data (), str1_size );
        internal_write ( version . data (), str2_size );

        state = new_state;        
    }


    int GeneralWriter :: addTable ( const std :: string &table_name )
    {        
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case schema_sent:
        case remote_name_and_schema_sent:
        case remote_name_schema_and_software_name_sent:
            new_state = have_table;
            break;
        case have_table:
        case have_column:
            new_state = state;
            break;
        default:
            throw "state violation adding table";
        }

        // create a pair between the outer db id ( 0 ) and this table name
        // this pair will act as an unique key of tables, whereas table_name
        // itself can be repeated if multiple databases are in use
        int_dbtbl tbl ( 0, table_name );

        // prediction this is the index
        int id = ( int ) tables.size() + 1;
        if ( id > 256 )
            throw "maximum number of tables exceeded";

        // make sure we never record a table name twice under the same db
        std :: pair < std :: map < int_dbtbl, int > :: iterator, bool > result = 
            table_name_idx.insert ( std :: pair < int_dbtbl, int > ( tbl, id ) );
        
        // if first time
        if ( result.second )
        {
            tables.push_back ( tbl );

            size_t str_size = table_name . size ();
            if ( str_size > 0x10000 )
                throw "maximum table name length exceeded";

            gwp_1string_evt_U16 hdr;
            init ( hdr, id, evt_new_table2 );
            set_size ( hdr, str_size );
            write_event ( & hdr . dad, sizeof hdr );
            internal_write ( table_name.data (), str_size );

            state = new_state;
        }
        
        // 1 based table id
        return result.first->second;
    }
    
    int GeneralWriter :: addColumn ( int table_id,
        const std :: string &column_name, uint32_t elem_bits, uint8_t flag_bits )
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
        
        if ( table_id <= 0 || ( size_t ) table_id > tables.size () )
            throw "Invalid table id";
        
        // the thing to insert into map
        // even if the caller wants us to use integer compaction,
        // it must be with a size we know how to use
        if ( ( flag_bits & 1 ) != 0 )
        {
            switch ( elem_bits )
            {
            case 16:
            case 32:
            case 64:
                break;
            default:
                flag_bits ^= 1;
            }
        }

        int_stream stream ( table_id, column_name, elem_bits, flag_bits );

        // prediction this is the index
        int id = ( int ) streams.size() + 1;
        if ( id > 256 )
            throw "maximum number of columns exceeded";

        // make sure we never record a column-spec twice
        std :: pair < std :: map < int_stream, int > :: iterator, bool > result =
            column_name_idx.insert ( std :: pair < int_stream, int > ( stream, id ) );
        
        // if first time
        if ( result.second )
        {
            streams.push_back ( stream );

            size_t str_size = column_name . size ();
            if ( str_size > 256 )
                throw "maximum column spec length exceeded";

            // TBD - write new column stream event to stream
            gwp_column_evt hdr;
            init ( hdr, id, evt_new_column );
            set_table_id ( hdr, table_id );
            set_elem_bits ( hdr, elem_bits );
            hdr.flag_bits = flag_bits;
            set_name_size ( hdr, str_size );

            // write header & data
            write_event ( & hdr . dad, sizeof hdr );
            internal_write ( column_name.data (), str_size );

            state = new_state;
        }
        
        // 1 based stream id
        return result.first->second;
    }

    int GeneralWriter :: dbAddDatabase ( int db_id, const std :: string &mbr_name, 
                                          const std :: string &db_name, uint8_t create_mode )
    {
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case schema_sent:
        case remote_name_and_schema_sent:
        case remote_name_schema_and_software_name_sent:
            new_state = have_table;
            break;
        case have_table:
        case have_column:
            new_state = state;
            break;
        default:
            throw "state violation adding db";
        }
        
        // zero ( 0 ) is a valid db_id
        if ( db_id < 0 || ( size_t ) db_id > dbs.size () )
            throw "Invalid database id";

        int_dbtbl db ( db_id, db_name );

        // prediction this is the index
        int id = ( int ) dbs.size() + 1;
        if ( id > 256 )
            throw "maximum number of databases exceeded";

        // make sure we never record a table name twice under the same db
        std :: pair < std :: map < int_dbtbl, int > :: iterator, bool > result = 
            db_name_idx.insert ( std :: pair < int_dbtbl, int > ( db, id ) );
        
        // if first time
        if ( result.second )
        {
            dbs.push_back ( db );

            size_t str_size = mbr_name . size ();
            if ( str_size > 0x100 )
                throw "maximum member name length exceeded";

            str_size = db_name . size ();
            if ( str_size > 0x100 )
                throw "maximum db name length exceeded";

            gwp_add_mbr_evt_v1 hdr;
            init ( hdr, id, evt_add_mbr_db );
            set_db_id ( hdr, db_id );
            set_size1 ( hdr, mbr_name.size () );
            set_size2 ( hdr, db_name.size () );
            set_create_mode ( hdr, create_mode );            
            write_event ( & hdr . dad, sizeof hdr );
            internal_write ( mbr_name.data (), mbr_name.size () );
            internal_write ( db_name.data (), db_name.size () );

            state = new_state;
        }
        
        // 1 based db id
        return result.first->second;
    }
    
    int GeneralWriter :: dbAddTable ( int db_id, const std :: string &mbr_name, 
                                       const std :: string &table_name, uint8_t create_mode )
    {
        stream_state new_state = uninitialized;

        switch ( state )
        {
        case schema_sent:
        case remote_name_and_schema_sent:
        case remote_name_schema_and_software_name_sent:
            new_state = have_table;
            break;
        case have_table:
        case have_column:
            new_state = state;
            break;
        default:
            throw "state violation adding db_table";
        }
        
        // zero ( 0 ) is a valid db_id
        if ( db_id < 0 || ( size_t ) db_id > dbs.size () )
            throw "Invalid database id";

        // create a pair between the outer db id ( 0 ) and this table name
        // this pair will act as an unique key of tables, whereas table_name
        // itself can be repeated if multiple databases are in use
        int_dbtbl tbl ( db_id, table_name );

        // prediction this is the index
        int id = ( int ) tables.size() + 1;
        if ( id > 256 )
            throw "maximum number of tables exceeded";

        // make sure we never record a table name twice under the same db
        std :: pair < std :: map < int_dbtbl, int > :: iterator, bool > result = 
            table_name_idx.insert ( std :: pair < int_dbtbl, int > ( tbl, id ) );
        
        // if first time
        if ( result.second )
        {
            tables.push_back ( tbl );

            size_t str_size = mbr_name . size ();
            if ( str_size > 0x100 )
                throw "maximum member name length exceeded";

            str_size = table_name . size ();
            if ( str_size > 0x100 )
                throw "maximum table name length exceeded";

            gwp_add_mbr_evt_v1 hdr;
            init ( hdr, id, evt_add_mbr_tbl );
            set_db_id ( hdr, db_id );
            set_size1 ( hdr, mbr_name.size () );
            set_size2 ( hdr, table_name.size () );
            set_create_mode ( hdr, create_mode );
            write_event ( & hdr . dad, sizeof hdr );
            internal_write ( mbr_name.data (), mbr_name.size () );
            internal_write ( table_name.data (), table_name.size () );

            state = new_state;
        }
        
        // 1 based table id
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

        gwp_evt_hdr hdr;
        init ( hdr, 0, evt_open_stream );

        // write header        
        write_event ( & hdr, sizeof hdr );

        state = new_state;
    }

    void GeneralWriter :: setDBMetadataNode ( int obj_id,
                                            const std :: string & node_path,
                                            const std :: string & value )
    {
        // zero ( 0 ) is a valid db_id
        if ( obj_id < 0 || ( size_t ) obj_id > dbs.size () )
            throw "Invalid database id";

        size_t str1_size = node_path . size ();
        if ( str1_size > STRING_LIMIT_16 )
            throw "DB_path too long";

        size_t str2_size = value . size ();
        if ( str2_size > STRING_LIMIT_16 )
            throw "value too long";

        if ( str1_size <= STRING_LIMIT_8 && str2_size <= STRING_LIMIT_8 )
        {
            // use 8-bit sizes
            gwp_2string_evt_v1 hdr;
            init ( hdr, obj_id, evt_db_metadata_node );
            set_size1 ( hdr, str1_size );
            set_size2 ( hdr, str2_size );
            write_event ( & hdr . dad, sizeof hdr );
        }
        else
        {
            // use 16-bit sizes
            gwp_2string_evt_U16_v1 hdr;
            init ( hdr, obj_id, evt_db_metadata_node2 );
            set_size1 ( hdr, str1_size );
            set_size2 ( hdr, str2_size );
            write_event ( & hdr . dad, sizeof hdr );
        }

        internal_write ( node_path . data (), str1_size );
        internal_write ( value . data (), str2_size );
    }

    void GeneralWriter :: setTblMetadataNode ( int obj_id,
                                            const std :: string & node_path,
                                            const std :: string & value )
    {
        if ( obj_id <= 0 || ( size_t ) obj_id > tables.size () )
            throw "Invalid table id";

        size_t str1_size = node_path . size ();
        if ( str1_size > STRING_LIMIT_16 )
            throw "tbl_path too long";

        size_t str2_size = value . size ();
        if ( str2_size > STRING_LIMIT_16 )
            throw "value too long";

        if ( str1_size <= STRING_LIMIT_8 && str2_size <= STRING_LIMIT_8 )
        {
            // use 8-bit sizes
            gwp_2string_evt_v1 hdr;
            init ( hdr, obj_id, evt_tbl_metadata_node );
            set_size1 ( hdr, str1_size );
            set_size2 ( hdr, str2_size );
            write_event ( & hdr . dad, sizeof hdr );
        }
        else
        {
            // use 16-bit sizes
            gwp_2string_evt_U16_v1 hdr;
            init ( hdr, obj_id, evt_tbl_metadata_node2 );
            set_size1 ( hdr, str1_size );
            set_size2 ( hdr, str2_size );
            write_event ( & hdr . dad, sizeof hdr );
        }

        internal_write ( node_path . data (), str1_size );
        internal_write ( value . data (), str2_size );

    }
    void GeneralWriter :: setColMetadataNode ( int obj_id,
                                            const std :: string & node_path,
                                            const std :: string & value )
    {
        if ( obj_id <= 0 || ( size_t ) obj_id > streams.size () )
            throw "Invalid column id";

        size_t str1_size = node_path . size ();
        if ( str1_size > STRING_LIMIT_16 )
            throw "tbl_path too long";

        size_t str2_size = value . size ();
        if ( str2_size > STRING_LIMIT_16 )
            throw "value too long";

        if ( str1_size <= STRING_LIMIT_8 && str2_size <= STRING_LIMIT_8 )
        {
            // use 8-bit sizes
            gwp_2string_evt_v1 hdr;
            init ( hdr, obj_id, evt_col_metadata_node );
            set_size1 ( hdr, str1_size );
            set_size2 ( hdr, str2_size );
            write_event ( & hdr . dad, sizeof hdr );
        }
        else
        {
            // use 16-bit sizes
            gwp_2string_evt_U16_v1 hdr;
            init ( hdr, obj_id, evt_col_metadata_node2 );
            set_size1 ( hdr, str1_size );
            set_size2 ( hdr, str2_size );
            write_event ( & hdr . dad, sizeof hdr );
        }

        internal_write ( node_path . data (), str1_size );
        internal_write ( value . data (), str2_size );
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

        if ( elem_bits == 0 )
            return;
        
        if ( data == 0 && elem_count != 0 )
            throw "Invalid data ptr";

        if ( elem_bits != streams [ stream_id - 1 ] . elem_bits )
            throw "Invalid elem_bits";

        size_t num_bytes = ( ( size_t ) elem_bits * elem_count + 7 ) / 8;
        if ( num_bytes == 0 )
        {
            gwp_evt_hdr_v1 eh;
            init ( eh, stream_id, evt_empty_default );
            write_event ( & eh, sizeof eh );
        }
        else
        {
            if ( num_bytes <= 256 )
            {
                gwp_data_evt chunk;
                init ( chunk, stream_id, evt_cell_default );
                set_size ( chunk, num_bytes );
                write_event ( & chunk . dad, sizeof chunk );
            }
            else if ( num_bytes <= 0x10000 )
            {
                gwp_data_evt_U16 chunk;
                init ( chunk, stream_id, evt_cell_default2 );
                set_size ( chunk, num_bytes );
                write_event ( & chunk . dad, sizeof chunk );
            }
            else
            {
                throw "default cell-data exceeds maximum";
            }
            internal_write ( data, num_bytes );
        }
    }

    template < class T >
    int encode_int ( T val, uint8_t * start, uint8_t * end );

    template <>
    int encode_int < uint16_t > ( uint16_t val, uint8_t * start, uint8_t * end )
    {
        int ret = encode_uint16 ( val, start, end );
        if ( ret > 0 )
        {
            uint16_t val2;
            int ret2 = decode_uint16 ( start, start + ret, & val2 );
            assert ( ret == ret2 && val == val2 );
        }
        return ret;
    }

    template <>
    int encode_int < uint32_t > ( uint32_t val, uint8_t * start, uint8_t * end )
    {
        int ret = encode_uint32 ( val, start, end );
        if ( ret > 0 )
        {
            uint32_t val2;
            int ret2 = decode_uint32 ( start, start + ret, & val2 );
            assert ( ret == ret2 && val == val2 );
        }
        return ret;
    }

    template <>
    int encode_int < uint64_t > ( uint64_t val, uint8_t * start, uint8_t * end )
    {
        int ret = encode_uint64 ( val, start, end );
        if ( ret > 0 )
        {
            uint64_t val2;
            int ret2 = decode_uint64 ( start, start + ret, & val2 );
            assert ( ret == ret2 && val == val2 );
        }
        return ret;
    }

    struct encode_result { uint32_t num_elems, num_bytes; };

    const size_t bsize = 0x10000;

    template < class T > static
    encode_result encode_buffer ( uint8_t * buffer, const void * data, uint32_t first, uint32_t elem_count )
    {
        uint8_t * start = buffer;
        uint8_t * end = buffer + bsize;
        const T * input = ( const T * ) data;

        uint32_t i;
        for ( i = first; i < elem_count; ++ i )
        {
            int num_writ = encode_int < T > ( input [ i ], start, end );
            if ( num_writ <= 0 )
            {
                if ( num_writ < 0 )
                    throw "error encoding integer data";
                break;
            }
            start += num_writ;
        }

        if ( start == buffer )
            throw "INTERNAL ERROR: no data to encode";

        encode_result rslt;
        rslt . num_elems = i;
        rslt . num_bytes = start - buffer;

        return rslt;
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

        const int_stream & s = streams [ stream_id - 1 ];

        if ( elem_bits != s . elem_bits )
            throw "Invalid elem_bits";

        bool compact_int = ( s . flag_bits & 1 ) != 0;
        
        const uint8_t * dp = ( const uint8_t * ) data;

        if ( compact_int )
        {
            uint32_t elem;
            encode_result rslt;
            encode_result ( * encode ) ( uint8_t * buffer, const void * data, uint32_t first, uint32_t elem_count );

            switch ( elem_bits )
            {
            case 16:
                encode = encode_buffer < uint16_t >;
                break;
            case 32:
                encode = encode_buffer < uint32_t >;
                break;
            case 64:
                encode = encode_buffer < uint64_t >;
                break;
            default:
                throw "INTERNAL ERROR: corrupt element bits";
            }

            for ( elem = 0; elem < elem_count; elem = rslt . num_elems )
            {
                rslt = ( * encode ) ( packing_buffer, data, elem, elem_count );
                if ( rslt . num_bytes <= 256 )
                {
                    assert ( rslt . num_bytes != 0 );
                    gwp_data_evt chunk;
                    init ( chunk, stream_id, evt_cell_data );
                    set_size ( chunk, rslt . num_bytes );
                    write_event ( & chunk . dad, sizeof chunk );
                }
                else
                {
                    gwp_data_evt_U16 chunk;
                    init ( chunk, stream_id, evt_cell_data2 );
                    set_size ( chunk, rslt . num_bytes );
                    write_event ( & chunk . dad, sizeof chunk );
                }
                internal_write ( packing_buffer, rslt . num_bytes );
            }
        }
        else
        {
            size_t num_bytes = ( ( size_t ) elem_bits * elem_count + 7 ) / 8;

            while ( num_bytes >= 0x10000 )
            {
                gwp_data_evt_U16 chunk;
                init ( chunk, stream_id, evt_cell_data2 );
                set_size ( chunk, 0x10000 );
                write_event ( & chunk . dad, sizeof chunk );
                internal_write ( dp, 0x10000 );
                num_bytes -= 0x10000;
                dp += 0x10000;
            }

            if ( num_bytes <= 256 )
            {
                gwp_data_evt chunk;
                init ( chunk, stream_id, evt_cell_data );
                set_size ( chunk, num_bytes );
                write_event ( & chunk . dad, sizeof chunk );
            }
            else
            {
                gwp_data_evt_U16 chunk;
                init ( chunk, stream_id, evt_cell_data2 );
                set_size ( chunk, num_bytes );
                write_event ( & chunk . dad, sizeof chunk );
            }
            
            internal_write ( data, num_bytes );
        }
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

        if ( table_id < 0 || ( size_t ) table_id > tables.size () )
            throw "Invalid table id";

        gwp_evt_hdr hdr;
        init ( hdr, table_id, evt_next_row );
        write_event ( & hdr, sizeof hdr );
    }


    void GeneralWriter :: moveAhead ( int table_id, uint64_t nrows )
    {
        switch ( state )
        {
        case opened:
            break;
        default:
            throw "state violation moving ahead nrows";
        }

        if ( table_id < 0 || ( size_t ) table_id > tables.size () )
            throw "Invalid table id";

        gwp_move_ahead_evt_v1 hdr;
        init ( hdr, table_id, evt_move_ahead );
        set_nrows ( hdr, nrows );
        write_event ( & hdr . dad, sizeof hdr );
    }

    void GeneralWriter :: logError ( const std :: string & msg )
    {
        switch ( state )
        {
        case header_written:
        case remote_name_sent:
        case schema_sent:
        case software_name_sent:
        case remote_name_and_schema_sent:
        case remote_name_and_software_name_sent:
        case schema_and_software_name_sent:
        case remote_name_schema_and_software_name_sent:
        case have_table:
        case have_column:
        case opened:
        case error:
            break;
        default:
            return;
        }

        gwp_1string_evt_U16 hdr;
        init ( hdr, 0, evt_errmsg2 );

        const char * msg_data = msg . data ();
        size_t str_size = msg . size ();
        if ( str_size == 0 )
        {
            msg_data = "ERROR: (NO MSG)";
            str_size = strlen ( msg_data );
        }
        else if ( str_size > 0x10000 )
            str_size = 0x10000;

        set_size ( hdr, str_size );
        write_event ( & hdr . dad, sizeof hdr );
        internal_write ( msg_data, str_size );
    }

    void GeneralWriter :: logMsg ( const std :: string &msg )
    {
        switch ( state )
        {
        case header_written:
        case remote_name_sent:
        case schema_sent:
        case software_name_sent:
        case remote_name_and_schema_sent:
        case remote_name_and_software_name_sent:
        case schema_and_software_name_sent:
        case remote_name_schema_and_software_name_sent:
        case have_table:
        case have_column:
        case opened:
            break;
        default:
            return;
        }

        size_t str_size = msg . size ();
        if ( str_size == 0 )
            return;

        if ( str_size > 0x10000 )
            str_size = 0x10000;

        gwp_1string_evt_U16 hdr;
        init ( hdr, 0, evt_logmsg );

        set_size ( hdr, str_size );
        write_event ( & hdr . dad, sizeof hdr );
        internal_write ( msg . data (), str_size );
    }

    void GeneralWriter :: progMsg ( const std :: string & name, uint32_t version,
        uint64_t done, uint64_t total )
    {
        switch ( state )
        {
        case opened:
            break;
        default:
            return;
        }
        
        size_t str_size = name . size ();
        if ( str_size == 0 )
            throw "zero-length app-name";
        if ( str_size > 0x100 )
            str_size = 0x100;

        // timestamp
        time_t timestamp = time ( NULL );

        if ( total == 0 )
            throw "illegal total value: would divide by zero";
        if ( done > total )
            throw "illegal done value: greater than total";
        
        // calculate percentage done
        double fpercent = ( double ) done / total;
        assert ( fpercent >= 0.0 && fpercent <= 100.0 );
        uint8_t percent = ( uint8_t ) ( fpercent * 100 );

        gwp_status_evt_v1 hdr;
        init ( hdr, 0, evt_progmsg );
        set_pid ( hdr, pid );
        set_version ( hdr, version );
        set_timestamp ( hdr, ( uint32_t ) timestamp );
        set_size ( hdr, str_size );
        set_percent ( hdr, percent );

        write_event ( &hdr . dad, sizeof hdr );
        internal_write ( name.data (), str_size );
    }

    void GeneralWriter :: endStream ()
    {
        switch ( state )
        {
        case header_written:
        case remote_name_sent:
        case schema_sent:
        case software_name_sent:
        case remote_name_and_schema_sent:
        case remote_name_and_software_name_sent:
        case schema_and_software_name_sent:
        case remote_name_schema_and_software_name_sent:
        case have_table:
        case have_column:
        case opened:
        case error:
            break;
        default:
            return;
        }

        gwp_evt_hdr hdr;
        init ( hdr, 0, evt_end_stream );
        write_event ( & hdr, sizeof hdr );

        state = closed;

        flush ();
    }

    
    // Constructors
    GeneralWriter :: GeneralWriter ( const std :: string &out_path )
        : out ( out_path.c_str(), std::ofstream::binary )
        , evt_count ( 0 )
        , byte_count ( 0 )
        , pid ( getpid () )
        , packing_buffer ( 0 )
        , output_buffer ( 0 )
        , output_bsize ( 0 )
        , output_marker ( 0 )
        , out_fd ( -1 )
        , state ( uninitialized )
    {
        packing_buffer = new uint8_t [ bsize ];
        writeHeader ();
    }

    
    // Constructors
    GeneralWriter :: GeneralWriter ( int _out_fd, size_t buffer_size )
        : evt_count ( 0 )
        , byte_count ( 0 )
        , pid ( getpid () )
        , packing_buffer ( 0 )
        , output_buffer ( 0 )
        , output_bsize ( buffer_size )
        , output_marker ( 0 )
        , out_fd ( _out_fd )
        , state ( uninitialized )
    {
        packing_buffer = new uint8_t [ bsize ];
        output_buffer = new uint8_t [ buffer_size ];
        writeHeader ();
    }
    
    GeneralWriter :: ~GeneralWriter ()
    {
        try
        {
            endStream ();
        }
        catch ( ... )
        {
        }

        delete [] output_buffer;
        delete [] packing_buffer;

        output_bsize = output_marker = 0;
        output_buffer = packing_buffer = 0;
    }

    bool GeneralWriter :: int_stream :: operator < ( const int_stream &s ) const
    {
        if ( table_id != s.table_id )
            return table_id < s.table_id;
        return column_name.compare ( s.column_name ) < 0;
    }
    
    GeneralWriter :: int_stream :: int_stream ( int _table_id, const std :: string &_column_name, uint32_t _elem_bits, uint8_t _flag_bits )
        : table_id ( _table_id )
        , column_name ( _column_name )
        , elem_bits ( _elem_bits )
        , flag_bits ( _flag_bits )
    {
    }

    bool GeneralWriter :: int_dbtbl :: operator < ( const int_dbtbl &db ) const
    {
        if ( db_id != db.db_id )
            return db_id < db.db_id;
        return obj_name.compare ( db.obj_name ) < 0;
    }

    GeneralWriter :: int_dbtbl :: int_dbtbl ( int _db_id, const std :: string &_obj_name )
        : db_id ( _db_id )
        , obj_name ( _obj_name )
    {
    }

    // Private methods

    uint32_t GeneralWriter :: getPid ()
    {
        return ( uint32_t ) pid;
    }

    void GeneralWriter :: writeHeader ()
    {
        :: gw_header_v1 hdr;
        init ( hdr );
        internal_write ( & hdr, sizeof hdr );
        state = header_written;

    }

    void GeneralWriter :: flush ()
    {
        if ( out_fd < 0 )
            out . flush ();
        else
        {
            ssize_t num_writ;
            for ( size_t total = 0; total < output_marker; total += num_writ )
            {
                num_writ = :: write ( out_fd, & output_buffer [ total ], output_marker - total );
                if ( num_writ < 0 )
                    throw "Error writing to fd";
                if ( num_writ == 0 )
                    throw "Transfer incomplete writing to fd";
            }

            output_marker = 0;
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
                size_t avail = output_bsize - output_marker;
                if ( avail == 0 )
                {
                    flush ();
                    avail = output_bsize - output_marker;
                }

                size_t to_write = num_bytes - total;
                if ( to_write > avail )
                    to_write = avail;

                assert ( to_write != 0 );
                memmove ( & output_buffer [ output_marker ], & p [ total ], to_write );
                output_marker += to_write;
                total += to_write;
            }

            byte_count += total;
        }
    }

    void GeneralWriter :: write_event ( const gwp_evt_hdr * e, size_t evt_size )
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

        assert ( evt ( * e ) != evt_bad_event );
        assert ( evt ( * e ) <  evt_max_id );

        internal_write ( e, evt_size );
    }

}
