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

#include "general-writer.h"
#include "utf8-like-int-codec.h"

#include <kfs/defs.h>

#include <iostream>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <stdlib.h>
#include <stdint.h>

using namespace ncbi;

namespace gw_dump
{
    static bool display;
    static uint32_t verbose;
    static uint64_t event_num;
    static uint64_t jump_event;
    static uint64_t end_event;
    static uint64_t foffset;

    struct tbl_entry
    {
        tbl_entry ( const std :: string & _name )
            : row_id ( 1 )
            , member_name ( _name )
            , tbl_name ( _name )
        {
        }

        tbl_entry ( const std :: string & _member_name, const std :: string & _tbl_name )
            : row_id ( 1 )
            , member_name ( _member_name )
            , tbl_name ( _tbl_name )
        {
        }

        int64_t row_id;
        std :: string member_name;
        std :: string tbl_name;
    };

    static std :: vector < tbl_entry > tbl_entries;

    struct db_entry
    {
        db_entry ( const std :: string & _member_name, const std :: string & _db_name )
            : row_id ( 1 )
            , member_name ( _member_name )
            , db_name ( _db_name )
        {
        }

        int64_t row_id;
        std :: string member_name;
        std :: string db_name;
    };

    static std :: vector < db_entry > db_entries;


    struct col_entry
    {
        col_entry ( uint32_t _table_id, const std :: string & name, uint32_t _elem_bits, uint8_t _flag_bits = 0 )
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

    /* read_1string
     */
    template < class T > static
    char * read_1string ( const T & eh, FILE * in )
    {
        size_t string_size = size ( eh );
        char * string_buffer = new char [ string_size ];
        size_t num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size, in );
        if ( num_read != string_size )
        {
            delete [] string_buffer;
            throw "failed to read string data";
        }

        return string_buffer;
    }

    template <>
    char * read_1string < :: gw_1string_evt_v1 > ( const :: gw_1string_evt_v1 & eh, FILE * in )
    {
        size_t string_size_uint32 = ( size ( eh ) + 3 ) / 4;
        uint32_t * string_buffer = new uint32_t [ string_size_uint32 ];
        size_t num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size_uint32, in );
        if ( num_read != string_size_uint32 )
        {
            delete [] string_buffer;
            throw "failed to read string data";
        }

        return ( char * ) string_buffer;
    }

    template <>
    char * read_1string < :: gw_status_evt_v1 > ( const :: gw_status_evt_v1 & eh, FILE * in )
    {
        size_t string_size_uint32 = ( size ( eh ) + 3 ) / 4;
        uint32_t * string_buffer = new uint32_t [ string_size_uint32 ];
        size_t num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size_uint32, in );
        if ( num_read != string_size_uint32 )
        {
            delete [] string_buffer;
            throw "failed to read string data";
        }

        return ( char * ) string_buffer;
    }

    /* whack_1string
     */
    template < class T > static
    void whack_1string ( const T & eh, char * string_buffer )
    {
        delete [] string_buffer;
    }

    template <>
    void whack_1string < :: gw_1string_evt_v1 > ( const :: gw_1string_evt_v1 & eh, char * string_buffer )
    {
        uint32_t * buffer = ( uint32_t * ) string_buffer;
        delete [] buffer;
    }

    /* read_2string
     */
    template < class T > static
    char * read_2string ( const T & eh, FILE * in )
    {
        size_t string_size = size1 ( eh ) + size2 ( eh );
        char * string_buffer = new char [ string_size ];
        size_t num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size, in );
        if ( num_read != string_size )
        {
            delete [] string_buffer;
            throw "failed to read dual string data";
        }

        return string_buffer;
    }

    template <>
    char * read_2string < :: gw_2string_evt_v1 > ( const :: gw_2string_evt_v1 & eh, FILE * in )
    {
        size_t string_size_uint32 = ( size1 ( eh ) + size2 ( eh ) + 3 ) / 4;
        uint32_t * string_buffer = new uint32_t [ string_size_uint32 ];
        size_t num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size_uint32, in );
        if ( num_read != string_size_uint32 )
        {
            delete [] string_buffer;
            throw "failed to read dual string data";
        }

        return ( char * ) string_buffer;
    }

    /* whack_2string
     */
    template < class T > static
    void whack_2string ( const T & eh, char * string_buffer )
    {
        delete [] string_buffer;
    }

    template <>
    void whack_2string < :: gw_2string_evt_v1 > ( const :: gw_2string_evt_v1 & eh, char * string_buffer )
    {
        uint32_t * buffer = ( uint32_t * ) string_buffer;
        delete [] buffer;
    }

    /* read_colname
     */
    template < class T > static
    char * read_colname ( const T & eh, FILE * in )
    {
        size_t string_size = name_size ( eh );
        char * string_buffer = new char [ string_size ];
        size_t num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size, in );
        if ( num_read != string_size )
        {
            delete [] string_buffer;
            throw "failed to read column name";
        }

        return string_buffer;
    }

    template <>
    char * read_colname < :: gw_column_evt_v1 > ( const :: gw_column_evt_v1 & eh, FILE * in )
    {
        size_t string_size_uint32 = ( name_size ( eh ) + 3 ) / 4;
        uint32_t * string_buffer = new uint32_t [ string_size_uint32 ];
        size_t num_read = readFILE ( string_buffer, sizeof string_buffer [ 0 ], string_size_uint32, in );
        if ( num_read != string_size_uint32 )
        {
            delete [] string_buffer;
            throw "failed to read column name";
        }

        return ( char * ) string_buffer;
    }

    /* whack_colname
     */
    template < class T > static
    void whack_colname ( const T & eh, char * string_buffer )
    {
        delete [] string_buffer;
    }

    template <>
    void whack_colname < :: gw_column_evt_v1 > ( const :: gw_column_evt_v1 & eh, char * string_buffer )
    {
        uint32_t * buffer = ( uint32_t * ) string_buffer;
        delete [] buffer;
    }


    /* check_move_ahead
     */
    template < class T > static
    void check_move_ahead ( const T & eh )
    {
        if ( id ( eh . dad ) == 0 )
            throw "bad table id within move-ahead event (null)";
        if ( id ( eh . dad ) > tbl_entries . size () )
            throw "bad table id within move-ahead event";
    }

    /* dump_move_ahead
     */
    template < class D, class T > static
    void dump_move_ahead ( FILE * in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( eh . nrows, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read move-ahead event";

        check_move_ahead ( eh );

        // advance row-id
        tbl_entry & te = tbl_entries [ id ( eh . dad ) - 1 ];
        te . row_id += get_nrows ( eh );

        if ( display )
        {
            const std :: string & tbl_name = te . tbl_name;

            std :: cout
                << event_num << ": move-ahead\n"
                << "  table_id = " << id ( eh . dad ) << " ( \"" << tbl_name << "\" )\n"
                << "  nrows = " << get_nrows ( eh ) << '\n'
                << "  row_id = " << te . row_id << '\n'
                ;
        }
    }


    /* check_next_row
     *  all:
     *    0 < id <= count(tbls)
     */
    template < class T > static
    void check_next_row ( const T & eh )
    {
        if ( id ( eh ) == 0 )
            throw "bad table id within next-row event (null)";
        if ( id ( eh ) > tbl_entries . size () )
            throw "bad table id within next-row event";
    }

    /* dump_next_row
     */
    template < class T > static
    void dump_next_row ( FILE * in, const T & eh )
    {
        check_next_row ( eh );

        // advance row-id
        tbl_entry & te = tbl_entries [ id ( eh ) - 1 ];
        ++ te . row_id;

        if ( display )
        {
            const std :: string & tbl_name = te . tbl_name;

            std :: cout
                << event_num << ": next-row\n"
                << "  table_id = " << id ( eh ) << " ( \"" << tbl_name << "\" )\n"
                << "  row_id = " << te . row_id << '\n'
                ;
        }
    }

    /* check_empty_default
     */
    template < class T > static
    void check_empty_default ( const T & eh )
    {
        if ( id ( eh ) == 0 )
            throw "bad cell event id (null)";
        if ( id ( eh ) > col_entries . size () )
            throw "bad cell event id";
    }

    /* dump_empty_default
     */
    template < class T > static
    void dump_empty_default ( FILE * in, const T & eh )
    {
        check_empty_default ( eh );

        if ( display )
        {
            const col_entry & entry = col_entries [ id ( eh ) - 1 ];
            const std :: string & tbl_name = tbl_entries [ entry . table_id - 1 ] . tbl_name;

            std :: cout
                << event_num << ": cell-default\n"
                << "  stream_id = " << id ( eh ) << " ( " << tbl_name << " . " << entry . spec << " )\n"
                << "  elem_bits = " << entry . elem_bits << '\n'
                << "  elem_count = 0 ( empty )\n"
                ;
        }
    }


    /* check_cell_event
     *  all:
     *    0 < id <= count ( columns )
     */
    template < class T > static
    void check_cell_event ( const T & eh )
    {
        if ( id ( eh . dad ) == 0 )
            throw "bad cell event id (null)";
        if ( id ( eh . dad ) > col_entries . size () )
            throw "bad cell event id";
    }

    /* check_int_packing
     *  deeply check contents for adherance to protocol
     */
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


    /* dump_cell_event
     */
    template < class D, class T > static
    void dump_cell_event ( FILE * in, const D & e, const char * type )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . sz, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read cell event";

        check_cell_event ( eh );

        size_t data_size = size ( eh );
        uint8_t * data_buffer = new uint8_t [ data_size ];
        num_read = readFILE ( data_buffer, sizeof data_buffer [ 0 ], data_size, in );
        if ( num_read != data_size )
        {
            delete [] data_buffer;
            throw "failed to read cell data";
        }

        bool packed_int = false;
        size_t unpacked_size = data_size;
        const col_entry & entry = col_entries [ id ( eh . dad ) - 1 ];

        if ( ( entry . flag_bits & 1 ) != 0 )
        {
            size_t data_size = size ( eh );

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

            packed_int = true;
        }

        if ( display )
        {
            const std :: string & tbl_name = tbl_entries [ entry . table_id - 1 ] . tbl_name;

            std :: cout
                << event_num << ": cell-" << type << '\n'
                << "  stream_id = " << id ( eh . dad ) << " ( " << tbl_name << " . " << entry . spec << " )\n"
                << "  elem_bits = " << entry . elem_bits << '\n'
                ;
            if ( packed_int )
            {
                std :: cout
                    << "  elem_count = " << ( unpacked_size * 8 ) / entry . elem_bits
                    << " ( " << unpacked_size << " bytes, " << data_size << " packed )\n"
                    ;
            }
            else
            {
                std :: cout
                    << "  elem_count = " << ( data_size * 8 ) / entry . elem_bits << " ( " << data_size << " bytes )\n"
                    ;
            }
        }

        delete [] data_buffer;
    }

    template <>
    void dump_cell_event < gw_evt_hdr_v1, gw_data_evt_v1 > ( FILE * in, const gw_evt_hdr_v1 & e, const char * type )
    {
        gw_data_evt_v1 eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . elem_count, sizeof eh - sizeof ( gw_evt_hdr_v1 ), 1, in );
        if ( num_read != 1 )
            throw "failed to read cell event";

        check_cell_event ( eh );

        const col_entry & entry = col_entries [ id ( eh . dad ) - 1 ];

        size_t data_size_uint32 = ( ( uint64_t ) entry . elem_bits * elem_count ( eh ) + 31 ) / 32;
        uint32_t * data_buffer = new uint32_t [ data_size_uint32 ];
        num_read = readFILE ( data_buffer, sizeof data_buffer [ 0 ], data_size_uint32, in );
        if ( num_read != data_size_uint32 )
        {
            delete [] data_buffer;
            throw "failed to read cell data";
        }

        if ( display )
        {
            const std :: string & tbl_name = tbl_entries [ entry . table_id - 1 ] . tbl_name;

            std :: cout
                << event_num << ": cell-" << type << '\n'
                << "  stream_id = " << id ( eh . dad ) << " ( " << tbl_name << " . " << entry . spec << " )\n"
                << "  elem_bits = " << entry . elem_bits << '\n'
                << "  elem_count = " << elem_count ( eh ) << '\n'
                ;
        }

        delete [] data_buffer;
    }


    /* check_open_stream
     */
    static
    void check_open_stream ( const gw_evt_hdr_v1 & eh )
    {
        if ( id ( eh ) != 0 )
            throw "non-zero id within open-stream event";
    }

    static
    void check_open_stream ( const gwp_evt_hdr_v1 & eh )
    {
    }


    /* dump_open_stream
     */
    template < class T > static
    void dump_open_stream ( FILE * in, const T & eh )
    {
        check_open_stream ( eh );

        if ( display )
        {
            std :: cout
                << event_num << ": open-stream\n"
                ;
        }
    }


    /* check_new_column
     *  all:
     *    id == count ( columns ) + 1
     *    0 < table_id <= count ( tbls )
     *    length ( name-spec ) != 0
     *  packed:
     *    flags in { 0, 1 }
     */
    static
    void check_new_column ( const gw_column_evt_v1 & eh )
    {
        if ( id ( eh . dad ) == 0 )
            throw "bad column/stream id";
        if ( ( size_t ) id ( eh . dad ) <= col_entries . size () )
            throw "column id already specified";
        if ( ( size_t ) id ( eh . dad ) - 1 > col_entries . size () )
            throw "column id out of order";
        if ( table_id ( eh ) == 0 )
            throw "bad column table-id (null)";
        if ( table_id ( eh ) > tbl_entries . size () )
            throw "bad column table-id";
        if ( name_size ( eh ) == 0 )
            throw "empty column name";
    }

    static
    void check_new_column ( const gwp_column_evt_v1 & eh )
    {
        if ( ( size_t ) id ( eh . dad ) <= col_entries . size () )
            throw "column id already specified";
        if ( ( size_t ) id ( eh . dad ) - 1 > col_entries . size () )
            throw "column id out of order";
        if ( table_id ( eh ) == 0 )
            throw "bad column table-id (null)";
        if ( table_id ( eh ) > tbl_entries . size () )
            throw "bad column table-id";
        if ( name_size ( eh ) == 0 )
            throw "empty column name";

        if ( ( eh . flag_bits & 0xFE ) != 0 )
            throw "uninitialized flag_bits";
    }


    /* dump_new_column
     */
    template < class D, class T > static
    void dump_new_column ( FILE * in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . table_id, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read new-column event";

        check_new_column ( eh );

        char * string_buffer = read_colname ( eh, in );
        std :: string name ( string_buffer, name_size ( eh ) );
        col_entries . push_back ( col_entry ( table_id ( eh ), name, elem_bits ( eh ), flag_bits ( eh ) ) );

        if ( display )
        {
            const std :: string & tbl_name = tbl_entries [ table_id ( eh ) - 1 ] . tbl_name;

            std :: cout
                << event_num << ": new-column\n"
                << "  table_id = " << table_id ( eh ) << " ( \"" << tbl_name << "\" )\n"
                << "  column_name [ " << name_size ( eh ) << " ] = \"" << name << "\"\n"
                ;
        }

        whack_colname ( eh, string_buffer );
    }

    /* check_new_table
     *  all:
     *    id == count ( tbls ) + 1
     *    length ( name ) != 0
     */
    template < class T > static
    void check_new_table ( const T & eh )
    {
        if ( id ( eh . dad ) == 0 )
            throw "bad table id";
        if ( ( size_t ) id ( eh . dad ) <= tbl_entries . size () )
            throw "table id already specified";
        if ( ( size_t ) id ( eh . dad ) - 1 > tbl_entries . size () )
            throw "table id out of order";
        if ( size ( eh ) == 0 )
            throw "empty table name";
    }

    /* dump_new_table
     */
    template < class D, class T > static
    void dump_new_table ( FILE * in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . sz, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read new-table event";

        check_new_table ( eh );

        char * string_buffer = read_1string ( eh, in );
        std :: string name ( string_buffer, size ( eh ) );
        tbl_entries . push_back ( tbl_entry ( name ) );

        if ( display )
        {
            std :: cout
                << event_num << ": new-table\n"
                << "  table_name [ " << size ( eh ) << " ] = \"" << name << "\"\n"
                ;
        }

        whack_1string ( eh, string_buffer );
    }

    /* check_use_schema
     *  non-packed:
     *    id == 0
     *  all:
     *    length ( schema-path ) != 0
     *    length ( schema-spec ) != 0
     */
    template < class T > static
    void check_use_schema ( const T & eh )
    {
        if ( size1 ( eh ) == 0 )
            throw "empty schema file path";
        if ( size2 ( eh ) == 0 )
            throw "empty schema spec";
    }

    template < >
    void check_use_schema < gw_2string_evt_v1 > ( const gw_2string_evt_v1 & eh )
    {
        if ( id ( eh . dad ) != 0 )
            throw "non-zero table id";
        if ( size1 ( eh ) == 0 )
            throw "empty schema file path";
        if ( size2 ( eh ) == 0 )
            throw "empty schema spec";
    }

    /* dump_use_schema
     */
    template < class D, class T > static
    void dump_use_schema ( FILE * in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . sz1, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read use-schema event";

        check_use_schema ( eh );

        char * string_buffer = read_2string ( eh, in );

        if ( display )
        {
            std :: string schema_file_name ( string_buffer, size1 ( eh ) );
            std :: string schema_db_spec ( & string_buffer [ size1 ( eh ) ], size2 ( eh ) );
            std :: cout
                << event_num << ": use-schema\n"
                << "  schema_file_name [ " << size1 ( eh ) << " ] = \"" << schema_file_name << "\"\n"
                << "  schema_db_spec [ " << size2 ( eh ) << " ] = \"" << schema_db_spec << "\"\n"
                ;
        }

        whack_2string ( eh, string_buffer );
    }

    /* check_software_name
     *  non-packed:
     *    id == 0
     *  all:
     *    length ( schema-path ) != 0
     *    length ( schema-spec ) != 0
     */

    void check_vers_component ( const char * vers, const char * end, long num, unsigned long max, char term )
    {
        if ( vers == end )
            throw "bad version";
        if ( * end != 0 && * end != term )
            throw "bad version";
        if ( num < 0 || num > max )
            throw "bad version";
    }

    void check_vers ( const char * vers )
    {
        char * end;
        long num = strtol ( vers, & end, 10 );
        check_vers_component ( vers, end, num, 255, '.' );
        if ( * end == '.' )
        {
            vers = end + 1;
            num = strtol ( vers, & end, 10 );
            check_vers_component ( vers, end, num, 255, '.' );
            if ( * end == '.' )
            {
                vers = end + 1;
                num = strtol ( vers, & end, 10 );
                check_vers_component ( vers, end, num, 0xFFFF, 0 );
            }
        }
    }

    template < class T > static
    void check_software_name ( const T & eh )
    {
        if ( size1 ( eh ) == 0 )
            throw "empty software name";
        if ( size2 ( eh ) == 0 )
            throw "empty version";
    }

    template < >
    void check_software_name < gw_2string_evt_v1 > ( const gw_2string_evt_v1 & eh )
    {
        if ( id ( eh . dad ) != 0 )
            throw "non-zero table id";
        if ( size1 ( eh ) == 0 )
            throw "empty software name";
        if ( size2 ( eh ) == 0 )
            throw "empty version";
    }

    /* dump_software_name
     */
    template < class D, class T > static
    void dump_software_name ( FILE * in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . sz1, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read software_name event";

        check_software_name ( eh );

        char * string_buffer = read_2string ( eh, in );
        std :: string software_name ( string_buffer, size1 ( eh ) );
        std :: string version ( & string_buffer [ size1 ( eh ) ], size2 ( eh ) );

        check_vers ( version . c_str () );

        if ( display )
        {
            std :: cout
                << event_num << ": software-name\n"
                << "  software_name [ " << size1 ( eh ) << " ] = \"" << software_name << "\"\n"
                << "  version [ " << size2 ( eh ) << " ] = \"" << version << "\"\n"
                ;
        }

        whack_2string ( eh, string_buffer );
    }

    /* check_metadata_node
     *
     */
    template < class T > static
    void check_metadata_node ( const T & eh )
    {
        if ( size1 ( eh ) == 0 )
            throw "empty metadata node";
        if ( size2 ( eh ) == 0 )
            throw "empty value";
    }

    template <>
    void check_metadata_node < gw_2string_evt_v1 > ( const gw_2string_evt_v1 & eh )
    {
        if ( size1 ( eh ) == 0 )
            throw "empty metadata node";
        if ( size2 ( eh ) == 0 )
            throw "empty value";
    }

    /* dump_metadata_node
     */
    template < class D, class T > static
    void dump_metadata_node ( FILE * in, const D & e )
    {        
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . sz1, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read metadata_node event";

        check_metadata_node ( eh );

        char *string_buffer = read_2string ( eh, in );
        std :: string node_path ( string_buffer, size1 ( eh ) );
        std :: string value ( & string_buffer [ size1 ( eh ) ], size2 ( eh ) );

        if ( display )
        {
            std :: cout 
                << event_num << ": metadata-node\n"
                << "  metadata_node [ " << size1 ( eh ) << " ] = \"" << node_path << "\"\n"
                << "  value [ " << size2 ( eh ) << " ] = \"" << value << "\"\n";
        }

        whack_1string ( eh, string_buffer );
    }

    /* check_add_mbr
     *
     */
    template < class T > static
    void check_add_mbr ( const T & eh )
    {
        if ( db_id ( eh ) < 0 || db_id ( eh ) > db_entries . size () )
             throw "invalid database id";
        if ( size1 ( eh ) == 0 )
            throw "empty member node";
        if ( size2 ( eh ) == 0 )
            throw "empty value";
        switch ( create_mode ( eh ) & kcmValueMask )
        {
            // only allowed mode values
            // or throw an exception
        case kcmOpen:
        case kcmInit:
        case kcmCreate:
            break;
        default:
            throw "invalid create mode";
        }
        if ( ( create_mode ( eh ) & ( kcmBitMask & ~ kcmMD5 & ~ kcmParents ) ) != 0 )
            throw "invalid create mode";
    }

    template <>
    void check_add_mbr < gw_add_mbr_evt_v1 > ( const gw_add_mbr_evt_v1 & eh )
    {
        if ( id ( eh . dad ) != 0 )
            throw "non-zero table id";
        if ( db_id ( eh ) < 0 || db_id ( eh ) > db_entries . size () )
             throw "invalid database id";
        if ( size1 ( eh ) == 0 )
            throw "empty member name";
        if ( size2 ( eh ) == 0 )
            throw "empty db/tbl name";
        switch ( create_mode ( eh ) & kcmValueMask )
        {
            // only allowed mode values
            // or throw an exception
        case kcmOpen:
        case kcmInit:
        case kcmCreate:
            break;
        default:
            throw "invalid create mode";
        }
        if ( ( create_mode ( eh ) & ( kcmBitMask & ~ kcmMD5 & ~ kcmParents ) ) != 0 )
            throw "invalid create mode";
    }

    /* dump_add_mbr
     */
    template < class D, class T > static
    void dump_add_mbr ( FILE * in, const D & e )
    {        
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . db_id, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read add_mbr event";

        check_add_mbr ( eh );

        uint32_t dbid = db_id ( eh );
        char *string_buffer = read_2string ( eh, in );
        std :: string member_name ( string_buffer, size1 ( eh ) );
        std :: string db_tbl_name ( & string_buffer [ size1 ( eh ) ], size2 ( eh ) );
        uint8_t cmode = create_mode ( eh ); 

        switch ( evt ( eh . dad ) )
        {
        case evt_add_mbr_db:
            db_entries . push_back ( db_entry ( member_name, db_tbl_name ) );
            break;
        case evt_add_mbr_tbl:
            tbl_entries . push_back ( tbl_entry ( member_name, db_tbl_name ) );
            break;
        default:
            throw "logic error";
        }

        if ( display )
        {
            std :: cout 
                << event_num << ": add-member\n"
                << "  db_id [ " << dbid << " ]\n"
                << "  add_mbr  [ " << size1 ( eh ) << " ] = \"" << member_name << "\"\n"
                << "  db/tbl  [ " << size2 ( eh ) << " ] = \"" << db_tbl_name << "\"\n"
                << "  create_mode  [ "
                << ( uint32_t ) cmode
                << " ] ( ";
            switch ( cmode & kcmValueMask )
            {
            case kcmOpen:
                std :: cout << "kcmOpen";
                break;
            case kcmInit:
                std :: cout << "kcmInit";
                break;
            case kcmCreate:
                std :: cout << "kcmCreate";
                break;
            }
            if ( ( cmode & kcmParents ) != 0 )
                std :: cout << ", kcmParents";
            if ( ( cmode & kcmMD5 ) != 0 )
                std :: cout << ", kcmMD5";
            std :: cout << " )\n";
        }

        whack_2string ( eh, string_buffer );
    }

    /* check_remote_path
     *  non-packed:
     *    id == 0
     *  all:
     *    length ( remote-path ) != 0
     */
    template < class T > static
    void check_remote_path ( const T & eh )
    {
        if ( size ( eh ) == 0 )
            throw "empty remote path";
    }

    template <>
    void check_remote_path < gw_1string_evt_v1 > ( const gw_1string_evt_v1 & eh )
    {
        if ( id ( eh . dad ) != 0 )
            throw "non-zero table id";
        if ( size ( eh ) == 0 )
            throw "empty remote path";
    }

    /* dump_remote_path
     */
    template < class D, class T > static
    void dump_remote_path ( FILE * in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . sz, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read remote-path event";

        check_remote_path ( eh );

        char * string_buffer = read_1string ( eh, in );

        if ( display )
        {
            std :: string path ( string_buffer, size ( eh ) );
            std :: cout
                << event_num << ": remote-path\n"
                << "  remote_db_name [ " << size ( eh ) << " ] = \"" << path << "\"\n"
                ;
        }

        whack_1string ( eh, string_buffer );
    }

    /* check_end_stream
     *  non-packed:
     *    id == 0
     */
    static
    void check_end_stream ( const gw_evt_hdr_v1 & eh )
    {
        if ( id ( eh ) != 0 )
            throw "non-zero id within end-stream event";
    }

    static
    void check_end_stream ( const gwp_evt_hdr_v1 & eh )
    {
    }

    /* dump_end_stream
     */
    template < class T > static
    bool dump_end_stream ( FILE * in, const T & eh )
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

    /* check_errmsg
     *  non-packed
     *    id == 0
     *  all:
     *    length ( msg ) != 0
     */
    template < class T > static
    void check_errmsg ( const T & eh )
    {
        if ( size ( eh ) == 0 )
            throw "empty error message";
    }

    template <>
    void check_errmsg < gw_1string_evt_v1 > ( const gw_1string_evt_v1 & eh )
    {
        if ( id ( eh . dad ) != 0 )
            throw "bad error-message id ( should be 0 )";
        if ( size ( eh ) == 0 )
            throw "empty error message";
    }

    /* dump_errmsg
     */
    template < class D, class T > static
    void dump_errmsg ( FILE * in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . sz, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read error-message event";

        check_errmsg ( eh );

        char * string_buffer = read_1string ( eh, in );

        if ( display )
        {
            std :: string msg ( string_buffer, size ( eh ) );

            std :: cout
                << event_num << ": error-message\n"
                << "  msg [ " << size ( eh ) << " ] = \"" << msg << "\"\n"
                ;
        }

        whack_1string ( eh, string_buffer );
    }

    /* check_logmsg
     *  non-packed
     *    id == 0
     *  all:
     *    length ( msg ) != 0
     */
    template < class T > static
    void check_logmsg ( const T & eh )
    {
        if ( size ( eh ) == 0 )
            throw "empty log message";
    }

    template <>
    void check_logmsg < gw_1string_evt_v1 > ( const gw_1string_evt_v1 & eh )
    {
        if ( id ( eh . dad ) != 0 )
            throw "bad log-message id ( should be 0 )";
        if ( size ( eh ) == 0 )
            throw "empty log message";
    }

    /* dump_logmsg
     */
    template < class D, class T > static
    void dump_logmsg ( FILE * in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & eh . sz, sizeof eh - sizeof ( D ), 1, in );
        if ( num_read != 1 )
            throw "failed to read log-message event";

        check_logmsg ( eh );

        char * string_buffer = read_1string ( eh, in );

        if ( display )
        {
            std :: string msg ( string_buffer, size ( eh ) );

            std :: cout
                << event_num << ": log-message\n"
                << "  msg [ " << size ( eh ) << " ] = \"" << msg << "\"\n"
                ;
        }

        whack_1string ( eh, string_buffer );
    }

    /* check_progmsg
     *  non-packed
     *    id == 0
     *  all:
     *    length ( msg ) != 0
     *    pid != 0 
     *    timestamp > 0
     *    version > 0 
     *    percent > 0 && < 100
     */
    template < class T > static
    void check_progmsg ( const T & eh )
    {
        if ( size ( eh ) == 0 )
            throw "empty prog message";
        if ( pid ( eh ) == 0 )
            throw "invalid pid";
        if ( timestamp ( eh ) == 0 )
            throw "empty timestamp";
        if ( version ( eh ) == 0 )
            throw "invalid version number";
        if ( percent ( eh ) < 0 || percent ( eh ) > 100 )
            throw "invalid percent";
    }

    template <>
    void check_progmsg < gw_status_evt_v1 > ( const gw_status_evt_v1 & eh )
    {
        if ( id ( eh . dad ) != 0 )
            throw "bad prog-message id ( should be 0 )";
        if ( size ( eh ) == 0 )
            throw "empty prog message";
        if ( pid ( eh ) == 0 )
            throw "invlaid pid";
        if ( timestamp ( eh ) == 0 )
            throw "empty timestamp";
        if ( version ( eh ) == 0 )
            throw "invalid version number";
        if ( percent ( eh ) < 0 || percent ( eh ) > 100 )
            throw "invalid percent";
    }

    /* dump-progmsg
     */
    template < class D, class T > static
    void dump_progmsg ( FILE *in, const D & e )
    {
        T eh;
        init ( eh, e );

        size_t num_read = readFILE ( & ( & eh . dad ) [ 1 ], sizeof eh - sizeof eh . dad, 1, in );
        if ( num_read != 1 )
            throw "failed to read prog-message event";

        check_progmsg ( eh );

        char *string_buffer = read_1string ( eh, in );
        std :: string app_name ( string_buffer, size ( eh ) );
        uint32_t _pid = pid ( eh );
        uint32_t _timestamp = timestamp ( eh );
        uint32_t _version = version ( eh );
        uint32_t _percent = percent ( eh );

        if ( display )
        {
            time_t ts = ( time_t ) _timestamp;
            char time_str [ 256 ];
            asctime_r ( localtime ( & ts ), time_str );
            size_t len = strlen ( time_str );
            while ( len > 0 && time_str [ len - 1 ] == '\n' )
                time_str [ -- len ] = 0;

            std :: cout 
                << event_num << ": prog-msg\n"
                << "  app [ " << app_name << " ] \n"
                << "  message [  proccessed " << _percent << "% ] \n"
                << "  pid [ " << _pid << " ]\n"
                << "  timestamp [ " << time_str << " ( " << _timestamp << " ) ] \n"
                << "  version [ " << ( _version >> 24 ) << '.' << ( ( _version >> 16 ) & 0xFF ) << '.' << ( _version & 0xFFFF ) << " ] \n"
                << "  percent [ " << _percent << " ]\n ";
        }

        whack_1string ( eh, string_buffer );
    }

    /* dump_v1_event
     *  the events are not packed
     */
    static
    bool dump_v1_event ( FILE * in )
    {
        if ( jump_event == event_num )
            display = true;
        else if ( end_event == event_num )
            display = false;

        gw_evt_hdr_v1 e;
        memset ( & e, 0, sizeof e );

        size_t num_read = readFILE ( & e, sizeof e, 1, in );
        if ( num_read != 1 )
        {
            int ch = fgetc ( in );
            if ( ch == EOF )
                throw "EOF before end-stream";

            throw "failed to read event";
        }
        switch ( evt ( e ) )
        {
        case evt_bad_event:
            throw "illegal event id - possibly block of zeros";
        case evt_errmsg:
            dump_errmsg < gw_evt_hdr_v1, gw_1string_evt_v1 > ( in, e );
            break;
        case evt_end_stream:
            return dump_end_stream ( in, e );
        case evt_remote_path:
            dump_remote_path < gw_evt_hdr_v1, gw_1string_evt_v1 > ( in, e );
            break;
        case evt_use_schema:
            dump_use_schema < gw_evt_hdr_v1, gw_2string_evt_v1 > ( in, e );
            break;
        case evt_new_table:
            dump_new_table < gw_evt_hdr_v1, gw_1string_evt_v1 > ( in, e );
            break;
        case evt_new_column:
            dump_new_column < gw_evt_hdr_v1, gw_column_evt_v1 > ( in, e );
            break;
        case evt_open_stream:
            dump_open_stream ( in, e );
            break;
        case evt_cell_default:
            dump_cell_event < gw_evt_hdr_v1, gw_data_evt_v1 > ( in, e, "default" );
            break;
        case evt_cell_data:
            dump_cell_event < gw_evt_hdr_v1, gw_data_evt_v1 > ( in, e, "data" );
            break;
        case evt_next_row:
            dump_next_row ( in, e );
            break;
        case evt_move_ahead:
            dump_move_ahead < gw_evt_hdr_v1, gw_move_ahead_evt_v1 > ( in, e );
            break;
        case evt_errmsg2:
        case evt_remote_path2:
        case evt_use_schema2:
        case evt_new_table2:
        case evt_cell_default2:
        case evt_cell_data2:
            throw "packed event id within non-packed stream";
        case evt_empty_default:
            dump_empty_default < gw_evt_hdr_v1 > ( in, e );
            break;

            // add in new message handlers for version 2
        case evt_software_name:
            dump_software_name < gw_evt_hdr_v1, gw_2string_evt_v1 > ( in, e );
            break;
        case evt_db_metadata_node:
            dump_metadata_node < gw_evt_hdr_v1, gw_2string_evt_v1 > ( in, e );
            break;
        case evt_tbl_metadata_node:
            dump_metadata_node < gw_evt_hdr_v1, gw_2string_evt_v1 > ( in, e );
            break;
        case evt_col_metadata_node:
            dump_metadata_node < gw_evt_hdr_v1, gw_2string_evt_v1 > ( in, e );
            break;
        case evt_db_metadata_node2:
        case evt_tbl_metadata_node2:
        case evt_col_metadata_node2:
            throw "packed event id within non-packed stream";
        case evt_add_mbr_db:
            dump_add_mbr < gw_evt_hdr_v1, gw_add_mbr_evt_v1 > ( in, e );
            break;
        case evt_add_mbr_tbl:
            dump_add_mbr < gw_evt_hdr_v1, gw_add_mbr_evt_v1 > ( in, e );
            break;

        case evt_logmsg:
            dump_logmsg < gw_evt_hdr_v1, gw_1string_evt_v1 > ( in, e );
            break;
        case evt_progmsg:
            dump_progmsg < gw_evt_hdr_v1, gw_status_evt_v1 > ( in, e );
            break;

        default:
            throw "unrecognized event id";
        }
        return true;
    }

    /* dump_v1_packed_event
     *  the events are all packed
     */
    static
    bool dump_v1_packed_event ( FILE * in )
    {
        if ( jump_event == event_num )
            display = true;
        else if ( end_event == event_num )
            display = false;

        gwp_evt_hdr_v1 e;
        memset ( & e, 0, sizeof e );

        size_t num_read = readFILE ( & e, sizeof e, 1, in );
        if ( num_read != 1 )
        {
            int ch = fgetc ( in );
            if ( ch == EOF )
                throw "EOF before end-stream";

            throw "failed to read event";
        }
        switch ( evt ( e ) )
        {
        case evt_bad_event:
            throw "illegal event id - possibly block of zeros";
        case evt_errmsg:
            dump_errmsg < gwp_evt_hdr_v1, gwp_1string_evt_v1 > ( in, e );
            break;
        case evt_end_stream:
            return dump_end_stream ( in, e );
        case evt_remote_path:
            dump_remote_path < gwp_evt_hdr_v1, gwp_1string_evt_v1 > ( in, e );
            break;
        case evt_use_schema:
            dump_use_schema < gwp_evt_hdr_v1, gwp_2string_evt_v1 > ( in, e );
            break;
        case evt_new_table:
            dump_new_table < gwp_evt_hdr_v1, gwp_1string_evt_v1 > ( in, e );
            break;
        case evt_new_column:
            dump_new_column < gwp_evt_hdr_v1, gwp_column_evt_v1 > ( in, e );
            break;
        case evt_open_stream:
            dump_open_stream ( in, e );
            break;
        case evt_cell_default:
            dump_cell_event < gwp_evt_hdr_v1, gwp_data_evt_v1 > ( in, e, "default" );
            break;
        case evt_cell_data:
            dump_cell_event < gwp_evt_hdr_v1, gwp_data_evt_v1 > ( in, e, "data" );
            break;
        case evt_next_row:
            dump_next_row ( in, e );
            break;
        case evt_move_ahead:
            dump_move_ahead < gwp_evt_hdr_v1, gwp_move_ahead_evt_v1 > ( in, e );
            break;
        case evt_errmsg2:
            dump_errmsg < gwp_evt_hdr_v1, gwp_1string_evt_U16_v1 > ( in, e );
            break;
        case evt_remote_path2:
            dump_remote_path < gwp_evt_hdr_v1, gwp_1string_evt_U16_v1 > ( in, e );
            break;
        case evt_use_schema2:
            dump_use_schema < gwp_evt_hdr_v1, gwp_2string_evt_U16_v1 > ( in, e );
            break;
        case evt_new_table2:
            dump_new_table < gwp_evt_hdr_v1, gwp_1string_evt_U16_v1 > ( in, e );
            break;
        case evt_cell_default2:
            dump_cell_event < gwp_evt_hdr_v1, gwp_data_evt_U16_v1 > ( in, e, "default" );
            break;
        case evt_cell_data2:
            dump_cell_event < gwp_evt_hdr_v1, gwp_data_evt_U16_v1 > ( in, e, "data" );
            break;
        case evt_empty_default:
            dump_empty_default < gwp_evt_hdr_v1 > ( in, e );
            break;

            // add in new message handlers for version 2
        case evt_software_name:
            dump_software_name < gwp_evt_hdr_v1, gwp_2string_evt_v1 > ( in, e );
            break;
        case evt_db_metadata_node:
            dump_metadata_node < gwp_evt_hdr_v1, gwp_2string_evt_v1 > ( in, e );
            break;
        case evt_tbl_metadata_node:
            dump_metadata_node < gwp_evt_hdr_v1, gwp_2string_evt_v1 > ( in, e );
            break;
        case evt_col_metadata_node:
            dump_metadata_node < gwp_evt_hdr_v1, gwp_2string_evt_v1 > ( in, e );
            break;
        case evt_db_metadata_node2:
            dump_metadata_node < gwp_evt_hdr_v1, gwp_2string_evt_v1 > ( in, e );
            break;
        case evt_tbl_metadata_node2:
            dump_metadata_node < gwp_evt_hdr_v1, gwp_2string_evt_v1 > ( in, e );
            break;
        case evt_col_metadata_node2:
            dump_metadata_node < gwp_evt_hdr_v1, gwp_2string_evt_v1 > ( in, e );
            break;
        case evt_add_mbr_db:
            dump_add_mbr < gwp_evt_hdr_v1, gwp_add_mbr_evt_v1 > ( in, e );
            break;
        case evt_add_mbr_tbl:
            dump_add_mbr < gwp_evt_hdr_v1, gwp_add_mbr_evt_v1 > ( in, e );
            break;
        case evt_logmsg:
            dump_logmsg < gwp_evt_hdr_v1, gwp_1string_evt_U16_v1 > ( in, e );
            break;
        case evt_progmsg:
            dump_progmsg < gwp_evt_hdr_v1, gwp_status_evt_v1 > ( in, e );
            break;

        default:
            throw "unrecognized packed event id";
        }
        return true;
    }

    static
    void check_v1_header ( const gw_header_v1 & hdr )
    {
        if ( hdr . packing > 1 )
            throw "bad packing spec";
    }

    static
    void dump_v1_header ( FILE * in, const gw_header & dad, bool & packed )
    {
        gw_header_v1 hdr;
        init ( hdr, dad );

        size_t num_read = readFILE ( & hdr . packing, sizeof hdr - sizeof ( gw_header ), 1, in );
        if ( num_read != 1 )
            throw "failed to read v1 header";

        check_v1_header ( hdr );

        if ( hdr . packing )
            packed = true;

        if ( display )
        {
            std :: cout
                << "header: version " << hdr . dad . version << '\n'
                << "  hdr_size = " << hdr . dad . hdr_size << '\n'
                << "  packing = " << hdr . packing << '\n'
                ;
        }
    }

    static
    void check_header ( const gw_header & hdr )
    {
        if ( memcmp ( hdr . signature, GW_SIGNATURE, sizeof hdr . signature ) != 0 )
            throw "bad header signature";
        if ( hdr . endian != GW_GOOD_ENDIAN )
        {
            if ( hdr . endian != GW_REVERSE_ENDIAN )
                throw "bad header byte order";
            throw "reversed header byte order";
        }
        if ( hdr . version < 1 )
            throw "bad header version";
        if ( hdr . version > GW_CURRENT_VERSION )
            throw "unknown header version";
    }

    static
    uint32_t dump_header ( FILE * in, bool & packed )
    {
        gw_header hdr;
        size_t num_read = readFILE ( & hdr, sizeof hdr, 1, in );
        if ( num_read != 1 )
            throw "failed to read header";

        check_header ( hdr );

        switch ( hdr . version )
        {
        case 1:
        case 2:
            dump_v1_header ( in, hdr, packed );
            break;
        default:
            throw "UNIMPLEMENTED: missing new version dumper";
        }
        return hdr . version;
    }

    static
    void dump ( FILE * in )
    {
        foffset = 0;

        bool packed = false;
        uint32_t version = dump_header ( in, packed );

        event_num = 1;
        switch ( version )
        {
        case 1:
        case 2:

            if ( packed )
            {
                while ( dump_v1_packed_event ( in ) )
                    ++ event_num;
            }
            else
            {
                while ( dump_v1_event ( in ) )
                    ++ event_num;
            }
            break;
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
