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

#ifndef _hpp_general_writer_
#define _hpp_general_writer_

#ifndef _h_general_writer_
#include "general-writer.h"
#endif

#include <iostream>
#include <string>
#include <stdint.h>
#include <vector>
#include <fstream>
#include <map>

#include <string.h>

namespace ncbi
{
    class GeneralWriter
    {
    public:

#if GW_CURRENT_VERSION >= 2

        // ask the general-loader to use this when naming its output
        void setRemotePath ( const std :: string & remote_db );

        // tell the general-loader to use this pre-defined schema
        void useSchema ( const std :: string & schema_file_name,
                         const std :: string & schema_db_spec );
#endif

        int addTable ( const std :: string &table_name );

#if GW_CURRENT_VERSION == 1
        int addColumn ( int table_id, const std :: string &column_name );
#else

        int addColumn ( int table_id, const std :: string &column_name, uint32_t elem_bits, uint8_t flags = 0 );

        inline int addIntegerColumn ( int table_id, const std :: string &column_name, uint32_t elem_bits )
        { return addColumn ( table_id, column_name, elem_bits, 1 ); }
#endif

        // ensure there are atleast one table and one column
        // set GeneralWriter to open state
        // write out open_stream event header
        void open ();

        // generates a chunk of cell data
        // MUST be entire default value in one event
        void  columnDefault ( int stream_id, uint32_t elem_bits, const void *data, uint32_t elem_count );

        // generate a chunk of cell data
        // may be repeated as often as necessary to complete a single cell's data
        void write ( int stream_id, uint32_t elem_bits, const void *data, uint32_t elem_count );

        // generate an event
        void nextRow ( int table_id );

        // indicate some sort of exception
        void logError ( const std :: string & msg );

        // generates an end event
        // puts object into state that will reject any further transmissions
        void endStream ();

        // out_path initializes output stream 
        // schema_name is recorded for the open
        GeneralWriter ( const std :: string &out_path,
                    const std :: string &remote_db,
                    const std :: string &schema_file_name, 
                    const std :: string &schema_db_spec );

        GeneralWriter ( int out_fd,
                    const std :: string &remote_db,
                    const std :: string &schema_file_name, 
                    const std :: string &schema_db_spec );

#if GW_CURRENT_VERSION >= 2
        GeneralWriter ( int out_fd );
        GeneralWriter ( const std :: string & out_path );
#endif

        // output stream is flushed and closed
        ~ GeneralWriter ();

    private:

        typedef gw_evt_id evt_id;
#if GW_CURRENT_VERSION == 1
        typedef gw_header_v1 header;
        typedef gw_evt_hdr_v1 evt_hdr;
        typedef gw_table_hdr_v1 table_hdr;
        typedef gw_column_hdr_v1 column_hdr;
        typedef gw_cell_hdr_v1 cell_hdr;
        typedef gw_errmsg_hdr_v1 errmsg_hdr;
#elif GW_CURRENT_VERSION >= 2
        typedef gw_header_v2 header;
        typedef gw_evt_hdr_v2 evt_hdr;
        typedef gw_string_hdr_U8_v2 table_hdr;
        typedef gw_column_hdr_v2 column_hdr;
        typedef gw_data_hdr_U8_v2 cell_hdr;
        typedef gw_string_hdr_U8_v2 errmsg_hdr;
#endif

        void writeHeader ();
        void internal_write ( const void *data, size_t num_bytes );

        void write_event ( const evt_hdr * evt, size_t evt_size );

        struct int_stream
        {
            bool operator < ( const int_stream &s ) const;
#if GW_CURRENT_VERSION == 1
            int_stream ( int table_id, const std :: string &column_name );
#else
            int_stream ( int table_id, const std :: string &column_name, uint32_t elem_bits, uint8_t flag_bits );
#endif

            int table_id;
            std :: string column_name;
#if GW_CURRENT_VERSION >= 2
            uint32_t elem_bits;
            uint8_t flag_bits;
#endif
        };

        std :: ofstream out;

#if GW_CURRENT_VERSION == 1
        std :: string remote_db;
        std :: string schema_file_name;
        std :: string schema_db_spec;
#endif

        std :: map < std :: string, int > table_name_idx;
        std :: map < int_stream, int > column_name_idx;

        std :: vector < int_stream > streams;
        std :: vector < std :: string > table_names;

        uint64_t evt_count;
        uint64_t byte_count;

#if GW_CURRENT_VERSION >= 2
        uint8_t * packing_buffer;
#endif

        int out_fd;

        enum stream_state
        {
            uninitialized,
            header_written,
            remote_name_sent,
            schema_sent,
            remote_name_and_schema_sent,
            have_table,
            have_column,
            opened,
            closed,
            error
        };
        stream_state state;
    };
}

// inlines
gw_header :: gw_header ( uint32_t vers )
    : endian ( GW_GOOD_ENDIAN )
    , version ( vers )
{
    memcpy ( signature, GW_SIGNATURE, sizeof signature );
}

gw_header :: gw_header ( const gw_header & hdr )
    : endian ( hdr . endian )
    , version ( hdr . version )
{
    memcpy ( signature, hdr . signature, sizeof signature );
}

// inlines for v1 structures
gw_header_v1 :: gw_header_v1 ()
    : gw_header ( 1 )
    , remote_db_name_size ( 0 )
    , schema_file_name_size ( 0 )
    , schema_db_spec_size ( 0 )
{
}

gw_header_v1 :: gw_header_v1 ( const gw_header & hdr )
    : gw_header ( hdr )
    , remote_db_name_size ( 0 )
    , schema_file_name_size ( 0 )
    , schema_db_spec_size ( 0 )
{
    if ( version != 1 )
        throw "bad version in copy constructor";
}

uint32_t gw_evt_hdr_v1 :: id () const
{
    return id_evt & 0xffffff;
}

gw_evt_id gw_evt_hdr_v1 :: evt () const
{
    return ( gw_evt_id ) ( id_evt >> 24 );
}

gw_evt_hdr_v1 :: gw_evt_hdr_v1 ( uint32_t id, gw_evt_id evt )
    : id_evt ( ( ( uint32_t ) evt << 24 ) | ( id & 0xffffff ) )
{
}

gw_evt_hdr_v1 :: gw_evt_hdr_v1 ( const gw_evt_hdr_v1 & hdr )
    : id_evt ( hdr . id_evt )
{
}

gw_table_hdr_v1 :: gw_table_hdr_v1 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v1 ( id, evt )
    , table_name_size ( 0 )
{
}

gw_table_hdr_v1 :: gw_table_hdr_v1 ( const gw_evt_hdr_v1 & hdr )
    : gw_evt_hdr_v1 ( hdr )
    , table_name_size ( 0 )
{
}

gw_column_hdr_v1 :: gw_column_hdr_v1 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v1 ( id, evt )
    , table_id ( 0 )
    , column_name_size ( 0 )
{
}

gw_column_hdr_v1 :: gw_column_hdr_v1 ( const gw_evt_hdr_v1 & hdr )
    : gw_evt_hdr_v1 ( hdr )
    , table_id ( 0 )
    , column_name_size ( 0 )
{
}

gw_cell_hdr_v1 :: gw_cell_hdr_v1 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v1 ( id, evt )
    , elem_bits ( 0 )
    , elem_count ( 0 )
{
}

gw_cell_hdr_v1 :: gw_cell_hdr_v1 ( const gw_evt_hdr_v1 & hdr )
    : gw_evt_hdr_v1 ( hdr )
    , elem_bits ( 0 )
    , elem_count ( 0 )
{
}

gw_errmsg_hdr_v1 :: gw_errmsg_hdr_v1 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v1 ( id, evt )
    , msg_size ( 0 )
{
}

gw_errmsg_hdr_v1 :: gw_errmsg_hdr_v1 ( const gw_evt_hdr_v1 & hdr )
    : gw_evt_hdr_v1 ( hdr )
    , msg_size ( 0 )
{
}

// inlines for v2 structures
gw_header_v2 :: gw_header_v2 ()
    : gw_header ( 2 )
    , hdr_size ( sizeof * this )
{
}

gw_header_v2 :: gw_header_v2 ( const gw_header & hdr )
    : gw_header ( hdr )
    , hdr_size ( sizeof * this )
{
    if ( version < 2 )
        throw "bad version in copy constructor";
}

uint32_t gw_evt_hdr_v2 :: id () const
{
    return ( uint32_t ) _id + 1;
}

gw_evt_id gw_evt_hdr_v2 :: evt () const
{
    return ( gw_evt_id ) _evt;
}

gw_evt_hdr_v2 :: gw_evt_hdr_v2 ( uint32_t id, gw_evt_id evt )
    : _evt ( evt )
    , _id ( id - 1 )
{
}

gw_evt_hdr_v2 :: gw_evt_hdr_v2 ( const gw_evt_hdr_v2 & hdr )
    : _evt ( hdr . _evt )
    , _id ( hdr . _id )
{
}

size_t gw_string_hdr_U8_v2 :: size () const
{
    return ( size_t ) string_size + 1;
}

gw_string_hdr_U8_v2 :: gw_string_hdr_U8_v2 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v2 ( id, evt )
    , string_size ( 0 )
{
}

gw_string_hdr_U8_v2 :: gw_string_hdr_U8_v2 ( const gw_evt_hdr_v2 & hdr )
    : gw_evt_hdr_v2 ( hdr )
    , string_size ( 0 )
{
}

size_t gw_string_hdr_U16_v2 :: size () const
{
    return ( size_t ) string_size + 1;
}

gw_string_hdr_U16_v2 :: gw_string_hdr_U16_v2 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v2 ( id, evt )
    , string_size ( 0 )
{
}

gw_string_hdr_U16_v2 :: gw_string_hdr_U16_v2 ( const gw_evt_hdr_v2 & hdr )
    : gw_evt_hdr_v2 ( hdr )
    , string_size ( 0 )
{
}

size_t gw_string2_hdr_U8_v2 :: size1 () const
{
    return ( size_t ) string1_size + 1;
}

size_t gw_string2_hdr_U8_v2 :: size2 () const
{
    return ( size_t ) string2_size + 1;
}

gw_string2_hdr_U8_v2 :: gw_string2_hdr_U8_v2 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v2 ( id, evt )
    , string1_size ( 0 )
    , string2_size ( 0 )
{
}

gw_string2_hdr_U8_v2 :: gw_string2_hdr_U8_v2 ( const gw_evt_hdr_v2 & hdr )
    : gw_evt_hdr_v2 ( hdr )
    , string1_size ( 0 )
    , string2_size ( 0 )
{
}

size_t gw_string2_hdr_U16_v2 :: size1 () const
{
    return ( size_t ) string1_size + 1;
}

size_t gw_string2_hdr_U16_v2 :: size2 () const
{
    return ( size_t ) string2_size + 1;
}

gw_string2_hdr_U16_v2 :: gw_string2_hdr_U16_v2 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v2 ( id, evt )
    , string1_size ( 0 )
    , string2_size ( 0 )
{
}

gw_string2_hdr_U16_v2 :: gw_string2_hdr_U16_v2 ( const gw_evt_hdr_v2 & hdr )
    : gw_evt_hdr_v2 ( hdr )
    , string1_size ( 0 )
    , string2_size ( 0 )
{
}

uint32_t gw_column_hdr_v2 :: table () const
{
    return ( uint32_t ) table_id + 1;
}

size_t gw_column_hdr_v2 :: size () const
{
    return ( size_t ) column_name_size + 1;
}

gw_column_hdr_v2 :: gw_column_hdr_v2 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v2 ( id, evt )
    , table_id ( 0 )
    , elem_bits ( 0 )
    , flag_bits ( 0 )
    , column_name_size ( 0 )
{
}

gw_column_hdr_v2 :: gw_column_hdr_v2 ( const gw_evt_hdr_v2 & hdr )
    : gw_evt_hdr_v2 ( hdr )
    , table_id ( 0 )
    , elem_bits ( 0 )
    , flag_bits ( 0 )
    , column_name_size ( 0 )
{
}

size_t gw_data_hdr_U8_v2 :: size () const
{
    return ( size_t ) data_size + 1;
}

gw_data_hdr_U8_v2 :: gw_data_hdr_U8_v2 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v2 ( id, evt )
    , data_size ( 0 )
{
}

gw_data_hdr_U8_v2 :: gw_data_hdr_U8_v2 ( const gw_evt_hdr_v2 & hdr )
    : gw_evt_hdr_v2 ( hdr )
    , data_size ( 0 )
{
}

size_t gw_data_hdr_U16_v2 :: size () const
{
    return ( size_t ) data_size + 1;
}

gw_data_hdr_U16_v2 :: gw_data_hdr_U16_v2 ( uint32_t id, gw_evt_id evt )
    : gw_evt_hdr_v2 ( id, evt )
    , data_size ( 0 )
{
}

gw_data_hdr_U16_v2 :: gw_data_hdr_U16_v2 ( const gw_evt_hdr_v2 & hdr )
    : gw_evt_hdr_v2 ( hdr )
    , data_size ( 0 )
{
}

#endif // _hpp_general_writer_
