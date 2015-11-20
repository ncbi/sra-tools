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
#if GW_CURRENT_VERSION <= 2
    typedef :: gwp_evt_hdr_v1 gwp_evt_hdr;
#else
#error "unrecognized GW version"
#endif

    class GeneralWriter
    {
    public:

        // ask the general-loader to use this when naming its output
        void setRemotePath ( const std :: string & remote_db );

        void setSoftwareName ( const std :: string & name,
                               const std :: string & version );

        // tell the general-loader to use this pre-defined schema
        void useSchema ( const std :: string & schema_file_name,
                         const std :: string & schema_db_spec );

        // add a new table
        // the table-id is returned
        int addTable ( const std :: string &table_name );

        // add a column to an existing table
        // the column-id is returned
        int addColumn ( int table_id, const std :: string &column_name, uint32_t elem_bits, uint8_t flags = 0 );

        // when the column is known to have integer data, use this method
        // it will utilize payload packing for reduced bandwidth
        inline int addIntegerColumn ( int table_id, const std :: string &column_name, uint32_t elem_bits )
        { return addColumn ( table_id, column_name, elem_bits, 1 ); }

        int dbAddDatabase ( int db_id, const std :: string &mbr_name, 
                             const std :: string &db_name, uint8_t create_mode );

        int dbAddTable ( int db_id, const std :: string &mbr_name, 
                             const std :: string &tbl_name, uint8_t create_mode );

        // ensure there are atleast one table and one column
        // set GeneralWriter to open state
        // write out open_stream event header
        void open ();

        // add or set metadata on a specific object
        // where obj_id == 0 => outer database, and
        // any other positive id means the database, table or column
        void setDBMetadataNode ( int obj_id,
                               const std :: string & node_path,
                               const std :: string & value );
        void setTblMetadataNode ( int obj_id,
                               const std :: string & node_path,
                               const std :: string & value );
        void setColMetadataNode ( int obj_id,
                               const std :: string & node_path,
                               const std :: string & value );

        // generates a chunk of cell data
        // MUST be entire default value in one event
        void columnDefault ( int stream_id, uint32_t elem_bits, const void *data, uint32_t elem_count );

        // generate a chunk of cell data
        // may be repeated as often as necessary to complete a single cell's data
        void write ( int stream_id, uint32_t elem_bits, const void *data, uint32_t elem_count );

        // commit and close current row, move to next row
        void nextRow ( int table_id );

        // commit and close current row, move ahead by nrows
        void moveAhead ( int table_id, uint64_t nrows );

        // indicate some sort of exception
        void logError ( const std :: string & msg );

        // XML logging for general-loader
        void logMsg ( const std :: string &msg );

        // indicate progress
        void progMsg ( const std :: string &name, 
                       uint32_t version, uint64_t done, uint64_t total );

        // generates an end event
        // puts object into state that will reject any further transmissions
        void endStream ();

        // out_fd writes to an open file descriptor ( generally stdout )
        // out_path initializes output stream for writing to a file
        GeneralWriter ( int out_fd, size_t buffer_size = 32 * 1024 );
        GeneralWriter ( const std :: string & out_path );

        // output stream is flushed and closed
        ~ GeneralWriter ();

    private:

        void writeHeader ();
        void internal_write ( const void *data, size_t num_bytes );
        void write_event ( const gwp_evt_hdr * evt, size_t evt_size );
        void flush ();
        uint32_t getPid ();

        struct int_stream
        {
            bool operator < ( const int_stream &s ) const;
            int_stream ( int table_id, const std :: string &column_name, uint32_t elem_bits, uint8_t flag_bits );

            int table_id;
            std :: string column_name;
            uint32_t elem_bits;
            uint8_t flag_bits;
        };

        struct int_dbtbl
        {
            bool operator < ( const int_dbtbl &db ) const;
            int_dbtbl ( int db_id, const std :: string &obj_name );

            int db_id;
            std :: string obj_name;
        };

        std :: ofstream out;

        std :: map < int_dbtbl, int > db_name_idx;
        std :: map < int_dbtbl, int > table_name_idx;
        std :: map < int_stream, int > column_name_idx;

        std :: vector < int_stream > streams;
        std :: vector < int_dbtbl > tables;
        std :: vector < int_dbtbl > dbs;

        uint64_t evt_count;
        uint64_t byte_count;

        int pid;

        uint8_t * packing_buffer;

        uint8_t * output_buffer;
        size_t output_bsize;
        size_t output_marker;

        int out_fd;

        enum stream_state
        {
            uninitialized,
            header_written,
            remote_name_sent,
            schema_sent,
            software_name_sent,
            remote_name_and_schema_sent,
            remote_name_and_software_name_sent,
            schema_and_software_name_sent,
            remote_name_schema_and_software_name_sent,
            have_table,
            have_column,
            opened,
            closed,
            error
        };
        stream_state state;
    };
}

#endif // _hpp_general_writer_
