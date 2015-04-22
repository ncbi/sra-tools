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
#include <iostream>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <stdlib.h>
#include <stdint.h>

namespace gw_dump
{
    static bool display;
    static uint32_t verbose;
    static uint64_t event_num;
    static uint64_t jump_event;
    static uint64_t end_event;
    static uint64_t foffset;

    static std :: vector < std :: string > tbl_names;
    struct col_entry
    {
        col_entry ( uint32_t _table_id, const std :: string & name )
            : table_id ( _table_id )
            , spec ( name )
            , elem_bits ( 0 )
            , flag_bits ( 0 )
        {
        }

        col_entry ( uint32_t _table_id, const std :: string & name, uint32_t _elem_bits, uint8_t _flag_bits )
            : table_id ( _table_id )
            , spec ( name )
            , elem_bits ( _elem_bits )
            , flag_bits ( _flag_bits )
        {
        }

        ~ col_entry () {}

        uint32_t table_id;
        std :: string spec;
        uint32_t elem_bits;
        uint8_t flag_bits;
    };
    static std :: vector < col_entry > col_entries;

    static
    size_t readFILE ( void * buffer, size_t elem_size, size_t elem_count, FILE * in )
    {
        size_t num_read = fread ( buffer, elem_size, elem_count, in );
        foffset += num_read * elem_size;
        return num_read;
    }

    // version 1

    static
    void check_errmsg ( const gw_errmsg_hdr_v1 & eh )
    {
        if ( eh . id () != 0 )
            throw "bad error-message id ( should be 0 )";
        if ( eh . msg_size == 0 )
            throw "empty error message";
    }

    static
    void dump_errmsg ( FILE * in, const gw_evt_hdr_v1 & e )
    {
        gw_errmsg_hdr_v1 eh ( e );

        size_t num_read = readFILE ( & eh . msg_size, sizeof eh - sizeof ( gw_evt_hdr_v1 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read error-message event";

        check_errmsg ( eh );

        size_t string_size_uint32 = ( ( eh . msg_size + 1 ) + 3 ) / 4;
        uint32_t * string_buffer = new uint32_t [ string_size_uint32 ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size_uint32, in );
        if ( num_read != string_size_uint32 )
        {
            delete [] string_buffer;
            throw "failed to read error-message";
        }

        if ( display )
        {
            const char * msg = ( const char * ) string_buffer;

            std :: cout
                << event_num << ": error-message\n"
                << "  msg [ " << eh . msg_size << " ] = \"" << msg << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    void check_next_row ( const gw_evt_hdr_v1 & eh )
    {
        if ( eh . id () == 0 )
            throw "bad table id within next-row event (null)";
        if ( eh . id () > tbl_names . size () )
            throw "bad table id within next-row event";
    }

    static
    void dump_next_row ( FILE * in, const gw_evt_hdr_v1 & eh )
    {
        check_next_row ( eh );

        if ( display )
        {
            std :: cout
                << event_num << ": next-row\n"
                << "  table_id = " << eh . id () << " ( \"" << tbl_names [ eh . id () - 1 ] << "\" )\n"
                ;
        }
    }

    static
    void check_cell_event ( const gw_cell_hdr_v1 & eh )
    {
        if ( eh . id () == 0 )
            throw "bad cell event id (null)";
        if ( eh . id () > col_entries . size () )
            throw "bad cell event id";
        if ( eh . elem_bits == 0 )
            throw "elem_bits 0 within cell-event";
        if ( ( eh . elem_bits & 7 ) != 0 )
            throw "non-byte-aligned elem_bits within cell-event";
    }

    static
    void dump_cell_event ( FILE * in, const gw_evt_hdr_v1 & e, const char * type )
    {
        gw_cell_hdr_v1 eh ( e );

        size_t num_read = readFILE ( & eh . elem_bits, sizeof eh - sizeof ( gw_evt_hdr_v1 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read cell event";

        check_cell_event ( eh );

        size_t data_size_uint32 = ( ( uint64_t ) eh . elem_bits * eh . elem_count + 31 ) / 32;
        uint32_t * data_buffer = new uint32_t [ data_size_uint32 ];
        num_read = readFILE ( data_buffer, sizeof data_buffer [ 0 ], data_size_uint32, in );
        if ( num_read != data_size_uint32 )
        {
            delete [] data_buffer;
            throw "failed to read cell data";
        }

        if ( display )
        {
            const col_entry & entry = col_entries [ eh . id () - 1 ];
            const std :: string & tbl_name = tbl_names [ entry . table_id - 1 ];

            std :: cout
                << event_num << ": cell-" << type << '\n'
                << "  stream_id = " << eh . id () << " ( " << tbl_name << " . " << entry . spec << " )\n"
                << "  elem_bits = " << eh . elem_bits << '\n'
                << "  elem_count = " << eh . elem_count << '\n'
                ;
        }

        delete [] data_buffer;
    }

    static
    void check_open_stream ( const gw_evt_hdr_v1 & eh )
    {
        if ( eh . id () != 0 )
            throw "non-zero id within open-stream event";
    }

    static
    void dump_open_stream ( FILE * in, const gw_evt_hdr_v1 & eh )
    {
        check_open_stream ( eh );

        if ( display )
        {
            std :: cout
                << event_num << ": open-stream\n"
                ;
        }
    }

    static
    void check_new_column ( const gw_column_hdr_v1 & eh )
    {
        if ( eh . id () == 0 )
            throw "bad column/stream id";
        if ( ( size_t ) eh . id () <= col_entries . size () )
            throw "column id already specified";
        if ( ( size_t ) eh . id () - 1 > col_entries . size () )
            throw "column id out of order";
        if ( eh . table_id == 0 )
            throw "bad column table-id (null)";
        if ( eh . table_id > tbl_names . size () )
            throw "bad column table-id";
        if ( eh . column_name_size == 0 )
            throw "empty column name";
    }

    static
    void dump_new_column ( FILE * in, const gw_evt_hdr_v1 & e )
    {
        gw_column_hdr_v1 eh ( e );

        size_t num_read = readFILE ( & eh . table_id, sizeof eh - sizeof ( gw_evt_hdr_v1 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read new-column event";

        check_new_column ( eh );

        size_t string_size_uint32 = ( ( eh . column_name_size + 1 ) + 3 ) / 4;
        uint32_t * string_buffer = new uint32_t [ string_size_uint32 ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size_uint32, in );
        if ( num_read != string_size_uint32 )
        {
            delete [] string_buffer;
            throw "failed to read column name";
        }

        const char * column_name = ( const char * ) string_buffer;
        std :: string name ( column_name );
        col_entries . push_back ( col_entry ( eh . table_id, name ) );

        if ( display )
        {
            std :: cout
                << event_num << ": new-column\n"
                << "  table_id = " << eh . table_id << " ( \"" << tbl_names [ eh . table_id - 1 ] << "\" )\n"
                << "  column_name [ " << eh . column_name_size << " ] = \"" << column_name << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    void check_new_table ( const gw_table_hdr_v1 & eh )
    {
        if ( eh . id () == 0 )
            throw "bad table id";
        if ( ( size_t ) eh . id () <= tbl_names . size () )
            throw "table id already specified";
        if ( ( size_t ) eh . id () - 1 > tbl_names . size () )
            throw "table id out of order";
        if ( eh . table_name_size == 0 )
            throw "empty table name";
    }

    static
    void dump_new_table ( FILE * in, const gw_evt_hdr_v1 & e )
    {
        gw_table_hdr_v1 eh ( e );

        size_t num_read = readFILE ( & eh . table_name_size, sizeof eh - sizeof ( gw_evt_hdr_v1 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read new-table event";

        check_new_table ( eh );

        size_t string_size_uint32 = ( ( eh . table_name_size + 1 ) + 3 ) / 4;
        uint32_t * string_buffer = new uint32_t [ string_size_uint32 ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size_uint32, in );
        if ( num_read != string_size_uint32 )
        {
            delete [] string_buffer;
            throw "failed to read table name";
        }

        const char * table_name = ( const char * ) string_buffer;
        std :: string name ( table_name );
        tbl_names . push_back ( name );

        if ( display )
        {
            std :: cout
                << event_num << ": new-table\n"
                << "  table_name [ " << eh . table_name_size << " ] = \"" << table_name << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    void check_end_stream ( const gw_evt_hdr_v1 & eh )
    {
        if ( eh . id () != 0 )
            throw "non-zero id within end-stream event";
    }

    static
    bool dump_end_stream ( FILE * in, const gw_evt_hdr_v1 & eh )
    {
        check_end_stream ( eh );
        if ( display )
        {
            std :: cout
                << "END\n"
                ;
        }
        return false;
    }

    static
    bool dump_v1_event ( FILE * in )
    {
        if ( jump_event == event_num )
            display = true;
        else if ( end_event == event_num )
            display = false;

        gw_evt_hdr_v1 e ( 0, evt_bad_event );
        size_t num_read = readFILE ( & e, sizeof e, 1, in );
        if ( num_read != 1 )
        {
            int ch = fgetc ( in );
            if ( ch == EOF )
                throw "EOF before end-stream";

            throw "failed to read event";
        }
        switch ( e . evt () )
        {
        case evt_bad_event:
            throw "illegal event id - possibly block of zeros";
        case evt_end_stream:
            return dump_end_stream ( in, e );
        case evt_new_table:
            dump_new_table ( in, e );
            break;
        case evt_new_column:
            dump_new_column ( in, e );
            break;
        case evt_open_stream:
            dump_open_stream ( in, e );
            break;
        case evt_cell_default:
            dump_cell_event ( in, e, "default" );
            break;
        case evt_cell_data:
            dump_cell_event ( in, e, "data" );
            break;
        case evt_next_row:
            dump_next_row ( in, e );
            break;
        case evt_errmsg:
            dump_errmsg ( in, e );
            break;
        default:
            throw "unrecognized event id";
        }
        return true;
    }

    // version 2

    static
    void check_errmsg ( const gw_string_hdr_U8_v2 & eh )
    {
    }

    static
    void dump_errmsg ( FILE * in, const gw_evt_hdr_v2 & e )
    {
        gw_string_hdr_U8_v2 eh ( e );

        size_t num_read = readFILE ( & eh . string_size, sizeof eh - sizeof ( gw_evt_hdr_v2 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read error-message event";

        check_errmsg ( eh );

        size_t string_size = eh . size () + 1;
        char * string_buffer = new char [ string_size ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size, in );
        if ( num_read != string_size )
        {
            delete [] string_buffer;
            throw "failed to read error-message";
        }
        if ( string_buffer [ string_size - 1 ] != 0 )
        {
            delete [] string_buffer;
            throw "string is not NUL-terminated";
        }

        if ( display )
        {
            const char * msg = string_buffer;

            std :: cout
                << event_num << ": error-message\n"
                << "  msg [ " << eh . size () << " ] = \"" << msg << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    void check_next_row ( const gw_evt_hdr_v2 & eh )
    {
        if ( eh . id () > tbl_names . size () )
            throw "bad table id within next-row event";
    }

    static
    void dump_next_row ( FILE * in, const gw_evt_hdr_v2 & eh )
    {
        check_next_row ( eh );

        if ( display )
        {
            std :: cout
                << event_num << ": next-row\n"
                << "  table_id = " << eh . id () << " ( \"" << tbl_names [ eh . id () - 1 ] << "\" )\n"
                ;
        }
    }

    static
    void check_cell_event ( const gw_data_hdr_U8_v2 & eh )
    {
        if ( eh . id () > col_entries . size () )
            throw "bad cell event id";
    }

    template < class T >
    int decode_int ( const uint8_t * start, const uint8_t * end, T * decoded );

    template <>
    int decode_int < uint16_t > ( const uint8_t * start, const uint8_t * end, uint16_t * decoded )
    {
        return decode_uint16 ( start, end, decoded );
    }

    template <>
    int decode_int < uint32_t > ( const uint8_t * start, const uint8_t * end, uint32_t * decoded )
    {
        return decode_uint32 ( start, end, decoded );
    }

    template <>
    int decode_int < uint64_t > ( const uint8_t * start, const uint8_t * end, uint64_t * decoded )
    {
        return decode_uint64 ( start, end, decoded );
    }

    template < class T > static
    size_t check_int_packing ( const uint8_t * data_buffer, size_t data_size )
    {
        const uint8_t * start = data_buffer;
        const uint8_t * end = data_buffer + data_size;

        size_t unpacked_size;
        for ( unpacked_size = 0; start < end; unpacked_size += sizeof ( T ) )
        {
            T decoded;
            int num_read = decode_int < T > ( start, end, & decoded );
            if ( num_read <= 0 )
            {
                switch ( num_read )
                {
                case CODEC_INSUFFICIENT_BUFFER:
                    throw "truncated data in packed integer buffer";
                case CODEC_INVALID_FORMAT:
                    throw "corrupt data in packed integer buffer";
                case CODEC_UNKNOWN_ERROR:
                    throw "unknown error in packed integer buffer";
                default:
                    throw "INTERNAL ERROR: decode_uintXX returned invalid error code";
                }
            }
            start += num_read;
        }

        return unpacked_size;
    }

    static
    void dump_cell_event ( FILE * in, const gw_evt_hdr_v2 & e, const char * type )
    {
        gw_data_hdr_U8_v2 eh ( e );

        size_t num_read = readFILE ( & eh . data_size, sizeof eh - sizeof ( gw_evt_hdr_v2 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read cell event";

        check_cell_event ( eh );

        size_t data_size = eh . size ();
        uint8_t * data_buffer = new uint8_t [ data_size ];
        num_read = readFILE ( data_buffer, sizeof data_buffer [ 0 ], data_size, in );
        if ( num_read != data_size )
        {
            delete [] data_buffer;
            throw "failed to read cell data";
        }

        bool packed_int = false;
        size_t unpacked_size = data_size;
        const col_entry & entry = col_entries [ eh . id () - 1 ];
        if ( ( entry . flag_bits & 1 ) != 0 )
        {
            packed_int = true;
            switch ( entry . elem_bits )
            {
            case 16:
                unpacked_size = check_int_packing < uint16_t > ( data_buffer, data_size );
                break;
            case 32:
                unpacked_size = check_int_packing < uint32_t > ( data_buffer, data_size );
                break;
            case 64:
                unpacked_size = check_int_packing < uint64_t > ( data_buffer, data_size );
                break;
            default:
                throw "bad element size for packed integer";
            }
        }

        if ( display )
        {
            const std :: string & tbl_name = tbl_names [ entry . table_id - 1 ];

            std :: cout
                << event_num << ": cell-" << type << '\n'
                << "  stream_id = " << eh . id () << " ( " << tbl_name << " . " << entry . spec << " )\n"
                << "  elem_bits = " << entry . elem_bits << '\n'
                ;
            if ( packed_int )
            {
                std :: cout
                    << "  elem_count = " << unpacked_size * 8 / entry . elem_bits
                    << " ( " << unpacked_size << " bytes, " << data_size << " packed )\n"
                    ;
            }
            else
            {
                std :: cout
                    << "  elem_count = " << data_size * 8 / entry . elem_bits << " ( " << data_size << " bytes )\n"
                    ;
            }
        }

        delete [] data_buffer;
    }

    static
    void check_cell2_event ( const gw_data_hdr_U16_v2 & eh )
    {
        if ( eh . id () > col_entries . size () )
            throw "bad cell event id";
    }

    static
    void dump_cell2_event ( FILE * in, const gw_evt_hdr_v2 & e, const char * type )
    {
        gw_data_hdr_U16_v2 eh ( e );

        size_t num_read = readFILE ( & eh . data_size, sizeof eh - sizeof ( gw_evt_hdr_v2 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read cell event";

        check_cell_event ( eh );

        size_t data_size = eh . size ();
        char * data_buffer = new char [ data_size ];
        num_read = readFILE ( data_buffer, sizeof data_buffer [ 0 ], data_size, in );
        if ( num_read != data_size )
        {
            delete [] data_buffer;
            throw "failed to read cell data";
        }

        if ( display )
        {
            const col_entry & entry = col_entries [ eh . id () - 1 ];
            const std :: string & tbl_name = tbl_names [ entry . table_id - 1 ];

            std :: cout
                << event_num << ": cell-" << type << '\n'
                << "  stream_id = " << eh . id () << " ( " << tbl_name << " . " << entry . spec << " )\n"
                << "  elem_bits = " << entry . elem_bits << '\n'
                << "  elem_count = " << data_size * 8 / entry . elem_bits << " ( " << data_size << " bytes )\n"
                ;
        }

        delete [] data_buffer;
    }

    static
    void check_open_stream ( const gw_evt_hdr_v2 & eh )
    {
    }

    static
    void dump_open_stream ( FILE * in, const gw_evt_hdr_v2 & eh )
    {
        check_open_stream ( eh );

        if ( display )
        {
            std :: cout
                << event_num << ": open-stream\n"
                ;
        }
    }

    static
    void check_new_column ( const gw_column_hdr_v2 & eh )
    {
        if ( ( size_t ) eh . id () <= col_entries . size () )
            throw "column id already specified";
        if ( ( size_t ) eh . id () - 1 > col_entries . size () )
            throw "column id out of order";
        if ( eh . table () > tbl_names . size () )
            throw "bad column table-id";
        if ( eh . elem_bits == 0 )
            throw "bad elem_bits";
        if ( ( eh . flag_bits & 0xFE ) != 0 )
            throw "uninitialized flag_bits";
    }

    static
    void dump_new_column ( FILE * in, const gw_evt_hdr_v2 & e )
    {
        gw_column_hdr_v2 eh ( e );

        size_t num_read = readFILE ( & eh . table_id, sizeof eh - sizeof ( gw_evt_hdr_v2 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read new-column event";

        check_new_column ( eh );

        size_t string_size = eh . size () + 1;
        char * string_buffer = new char [ string_size ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size, in );
        if ( num_read != string_size )
        {
            delete [] string_buffer;
            throw "failed to read column name";
        }
        if ( string_buffer [ string_size - 1 ] != 0 )
        {
            delete [] string_buffer;
            throw "string is not NUL-terminated";
        }

        const char * column_name = ( const char * ) string_buffer;
        std :: string name ( column_name );
        col_entries . push_back ( col_entry ( eh . table (), name, eh . elem_bits, eh . flag_bits ) );

        if ( display )
        {
            std :: cout
                << event_num << ": new-column\n"
                << "  table_id = " << eh . table () << " ( \"" << tbl_names [ eh . table () - 1 ] << "\" )\n"
                << "  column_name [ " << eh . size () << " ] = \"" << column_name << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    void check_new_table ( const gw_string_hdr_U8_v2 & eh )
    {
        if ( ( size_t ) eh . id () <= tbl_names . size () )
            throw "table id already specified";
        if ( ( size_t ) eh . id () - 1 > tbl_names . size () )
            throw "table id out of order";
    }

    static
    void dump_new_table ( FILE * in, const gw_evt_hdr_v2 & e )
    {
        gw_string_hdr_U8_v2 eh ( e );

        size_t num_read = readFILE ( & eh . string_size, sizeof eh - sizeof ( gw_evt_hdr_v2 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read new-table event";

        check_new_table ( eh );

        size_t string_size = eh . size () + 1;
        char * string_buffer = new char [ string_size ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size, in );
        if ( num_read != string_size )
        {
            delete [] string_buffer;
            throw "failed to read table name";
        }
        if ( string_buffer [ string_size - 1 ] != 0 )
        {
            delete [] string_buffer;
            throw "string is not NUL-terminated";
        }

        const char * table_name = ( const char * ) string_buffer;
        std :: string name ( table_name );
        tbl_names . push_back ( name );

        if ( display )
        {
            std :: cout
                << event_num << ": new-table\n"
                << "  table_name [ " << eh . size () << " ] = \"" << table_name << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    void check_end_stream ( const gw_evt_hdr_v2 & eh )
    {
    }

    static
    bool dump_end_stream ( FILE * in, const gw_evt_hdr_v2 & eh )
    {
        check_end_stream ( eh );
        if ( display )
        {
            std :: cout
                << "END\n"
                ;
        }
        return false;
    }

    static
    void check_use_schema ( const gw_string2_hdr_U16_v2 & eh )
    {
    }

    static
    void dump_use_schema ( FILE * in, const gw_evt_hdr_v2 & e )
    {
        gw_string2_hdr_U16_v2 eh ( e );

        size_t num_read = readFILE ( & eh . string1_size, sizeof eh - sizeof ( gw_evt_hdr_v2 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read use-schema event";

        check_use_schema ( eh );

        size_t string_size = eh . size1 () + 1 + eh . size2 () + 1;
        char * string_buffer = new char [ string_size ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size, in );
        if ( num_read != string_size )
        {
            delete [] string_buffer;
            throw "failed to read use-schema string";
        }
        if ( string_buffer [ eh . size1 () ] != 0 || string_buffer [ string_size - 1 ] != 0 )
        {
            delete [] string_buffer;
            throw "string is not NUL-terminated";
        }

        if ( display )
        {
            const char * schema_file_name = string_buffer;
            const char * schema_db_spec = & string_buffer [ eh . size1 () + 1 ];
            std :: cout
                << event_num << ": use-schema\n"
                << "  schema_file_name [ " << eh . size1 () << " ] = \"" << schema_file_name << "\"\n"
                << "  schema_db_spec [ " << eh . size2 () << " ] = \"" << schema_db_spec << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    void check_remote_path ( const gw_string_hdr_U16_v2 & eh )
    {
    }

    static
    void dump_remote_path ( FILE * in, const gw_evt_hdr_v2 & e )
    {
        gw_string_hdr_U16_v2 eh ( e );

        size_t num_read = readFILE ( & eh . string_size, sizeof eh - sizeof ( gw_evt_hdr_v2 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read remote-path event";

        check_remote_path ( eh );

        size_t string_size = eh . size () + 1;
        char * string_buffer = new char [ string_size ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size, in );
        if ( num_read != string_size )
        {
            delete [] string_buffer;
            throw "failed to read table name";
        }
        if ( string_buffer [ string_size - 1 ] != 0 )
        {
            delete [] string_buffer;
            throw "string is not NUL-terminated";
        }

        if ( display )
        {
            std :: cout
                << event_num << ": remote-path\n"
                << "  remote_db_name [ " << eh . size () << " ] = \"" << string_buffer << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    bool dump_v2_event ( FILE * in )
    {
        if ( jump_event == event_num )
            display = true;
        else if ( end_event == event_num )
            display = false;

        gw_evt_hdr_v2 e ( 0, evt_bad_event );
        size_t num_read = readFILE ( & e, sizeof e, 1, in );
        if ( num_read != 1 )
        {
            int ch = fgetc ( in );
            if ( ch == EOF )
                throw "EOF before end-stream";

            throw "failed to read event";
        }
        switch ( e . evt () )
        {
        case evt_bad_event:
            throw "illegal event id - possibly block of zeros";
        case evt_end_stream:
            return dump_end_stream ( in, e );
        case evt_new_table:
            dump_new_table ( in, e );
            break;
        case evt_new_column:
            dump_new_column ( in, e );
            break;
        case evt_open_stream:
            dump_open_stream ( in, e );
            break;
        case evt_cell_default:
            dump_cell_event ( in, e, "default" );
            break;
        case evt_cell_data:
            dump_cell_event ( in, e, "data" );
            break;
        case evt_next_row:
            dump_next_row ( in, e );
            break;
        case evt_errmsg:
            dump_errmsg ( in, e );
            break;
        case evt_remote_path:
            dump_remote_path ( in, e );
            break;
        case evt_use_schema:
            dump_use_schema ( in, e );
            break;
        case evt_cell2_default:
            dump_cell2_event ( in, e, "default" );
            break;
        case evt_cell2_data:
            dump_cell2_event ( in, e, "data" );
            break;
        default:
            throw "unrecognized event id";
        }
        return true;
    }

    static
    void check_v1_header ( const gw_header_v1 & hdr )
    {
        if ( hdr . remote_db_name_size == 0 )
            throw "empty remote_db_name";
        if ( hdr . schema_file_name_size == 0 )
            throw "empty schema_file_name";
        if ( hdr . schema_db_spec_size == 0 )
            throw "empty schema_db_spec";
    }

    static
    void dump_v1_header ( FILE * in, const gw_header & dad )
    {
        gw_header_v1 hdr ( dad );

        size_t num_read = readFILE ( & hdr . remote_db_name_size, sizeof hdr - sizeof ( gw_header ), 1, in );
        if ( num_read != 1 )
            throw "failed to read v1 header";

        check_v1_header ( hdr );

        size_t data_size_uint32
            = ( ( hdr . remote_db_name_size
                  + hdr . schema_file_name_size
                  + hdr . schema_db_spec_size
                  + 3 ) + 3 ) / 4;

        uint32_t * string_buffer = new uint32_t [ data_size_uint32 ];
        num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], data_size_uint32, in );
        if ( num_read != data_size_uint32 )
        {
            delete [] string_buffer;
            throw "failed to read header string data";
        }

        if ( display )
        {
            const char * remote_db_name = ( const char * ) string_buffer;
            const char * schema_file_name = remote_db_name + hdr . remote_db_name_size + 1;
            const char * schema_db_spec = schema_file_name + hdr . schema_file_name_size + 1;
            std :: cout
                << "header: version " << hdr . version << '\n'
                << "  remote_db_name [ " << hdr . remote_db_name_size << " ] = \"" << remote_db_name << "\"\n"
                << "  schema_file_name [ " << hdr . schema_file_name_size << " ] = \"" << schema_file_name << "\"\n"
                << "  schema_db_spec [ " << hdr . schema_db_spec_size << " ] = \"" << schema_db_spec << "\"\n"
                ;
        }

        delete [] string_buffer;
    }

    static
    void check_v2_header ( const gw_header_v2 & hdr )
    {
        if ( hdr . hdr_size != sizeof ( gw_header_v2 ) )
            throw "bad header size";
    }

    static
    void dump_v2_header ( FILE * in, const gw_header & dad )
    {
        gw_header_v2 hdr ( dad );

        size_t num_read = readFILE ( & hdr . hdr_size, sizeof hdr - sizeof ( gw_header ), 1, in );
        if ( num_read != 1 )
            throw "failed to read v2 header";

        check_v2_header ( hdr );

        if ( display )
        {
            std :: cout
                << "header: version " << hdr . version << '\n'
                << "  hdr_size = " << hdr . hdr_size << '\n'
                ;
        }
    }

    static
    void check_header ( const gw_header & hdr )
    {
        if ( memcmp ( hdr . signature, "NCBIgnld", 8 ) != 0 )
            throw "bad header signature";
        if ( hdr . endian != 1 )
        {
            if ( bswap_32 ( hdr . endian ) != 1 )
                throw "bad header byte order";
            throw "reversed header byte order";
        }
        if ( hdr . version < 1 )
            throw "bad header version";
        if ( hdr . version > 2 )
            throw "unknown header version";
    }

    static
    uint32_t dump_header ( FILE * in )
    {
        gw_header hdr;
        size_t num_read = readFILE ( & hdr, sizeof hdr, 1, in );
        if ( num_read != 1 )
            throw "failed to read header";

        check_header ( hdr );

        if ( hdr . version == 1 )
            dump_v1_header ( in, hdr );
        else
            dump_v2_header ( in, hdr );
        return hdr . version;
    }

    static
    void dump ( FILE * in )
    {
        foffset = 0;

        uint32_t version = dump_header ( in );

        event_num = 1;
        if ( version == 1 )
        {
            while ( dump_v1_event ( in ) )
                ++ event_num;
        }
        else
        {
            while ( dump_v2_event ( in ) )
                ++ event_num;
        }

        int ch = fgetc ( in );
        if ( ch != EOF )
            throw "excess data after end-stream";
    }

    static
    const char * nextArg ( int & i, int argc, char * argv [] )
    {
        if ( ++ i >= argc )
            throw "expected argument";
        return argv [ i ];
    }

    static
    const char * nextArg ( const char * & argp, int & i, int argc, char * argv [] )
    {
        const char * arg = argp;
        argp = "\0";

        if ( arg [ 1 ] != 0 )
            return arg + 1;

        return nextArg ( i, argc, argv );
    }

    static
    uint64_t atoU64 ( const char * str )
    {
        char * end;
        long i = strtol ( str, & end, 0 );
        if ( end [ 0 ] != 0 )
            throw "badly formed number";
        if ( i < 0 )
            throw "number out of bounds";
        return ( uint64_t ) i;
    }

    static
    void help ( const char * tool_path )
    {
        const char * tool_name = strrchr ( tool_path, '/' );
        if ( tool_name ++ == NULL )
            tool_name = tool_path;

        std :: cout
            << '\n'
            << "Usage:\n"
            << "  " << tool_name << " [options] [<stream-file> ...]\n"
            << '\n'
            << "Summary:\n"
            << "  This is a tool to analyze and display the contents of a stream produced by\n"
            << "  the \"general-writer\" library.\n"
            << '\n'
            << "  Input may be taken from stdin ( DEFAULT ) or from one or more stream-files.\n"
            << '\n'
            << "Options:\n"
            << "  -j event-num                     jump to numbered event before displaying.\n"
            << "                                   ( event numbers are 1-based, so the first event is 1. )\n"
            << "  -N event-count                   display a limited number of events and then go quiet.\n"
            << "  -v                               increase verbosity. Use multiple times for increased verbosity.\n"
            << "                                   ( currently this only enables or disables display. )\n"
            << "  -h                               display this help message\n"
            << '\n'
            << tool_path << '\n'
            ;
    }

    static
    void run ( int argc, char * argv [] )
    {
        uint32_t num_files = 0;

        for ( int i = 1; i < argc; ++ i )
        {
            const char * arg = argv [ i ];
            if ( arg [ 0 ] != '-' )
                argv [ ++ num_files ] = ( char * ) arg;
            else do switch ( ( ++ arg ) [ 0 ] )
            {
            case 'v':
                ++ verbose;
                break;
            case 'j':
                jump_event = atoU64 ( nextArg ( arg, i, argc, argv ) );
                break;
            case 'N':
                end_event = atoU64 ( nextArg ( arg, i, argc, argv ) );
                break;
            case 'h':
            case '?':
                help ( argv [ 0 ] );
                return;
            default:
                throw "unrecognized option";
            }
            while ( arg [ 1 ] != 0 );
        }

        if ( verbose && jump_event == 0 )
            display = true;

        end_event += jump_event;

        if ( num_files == 0 )
            dump ( stdin );
        else for ( uint32_t i = 1; i <= num_files; ++ i )
        {
            FILE * in = fopen ( argv [ i ], "rb" );
            if ( in == 0 )
                std :: cerr << "WARNING: failed to open input file: \"" << argv [ i ] << '\n';
            else
            {
                dump ( in );
                fclose ( in );
            }
        }
    }
}

int main ( int argc, char * argv [] )
{
    int status = 1;
    try
    {
        gw_dump :: run ( argc, argv );
        status = 0;
    }
    catch ( const char x [] )
    {
        std :: cerr
            << "ERROR: offset "
            << gw_dump :: foffset
            << ": event "
            << gw_dump :: event_num
            << ": "
            << x
            << '\n'
            ;
    }
    catch ( ... )
    {
        std :: cerr
            << "ERROR: offset "
            << gw_dump :: foffset
            << ": event "
            << gw_dump :: event_num
            << ": unknown error\n"
            ;
    }

    return status;
}
