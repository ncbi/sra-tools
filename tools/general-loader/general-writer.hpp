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

namespace ncbi
{
    class GeneralWriter
    {
    public:

        int addTable ( const std :: string &table_name );

        int addColumn ( int table_id, const std :: string &column_name );

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


        // output stream is flushed and closed
        ~ GeneralWriter ();

    private:

        typedef gw_header header;
        typedef gw_table_hdr table_hdr;
        typedef gw_column_hdr column_hdr;
        typedef gw_cell_hdr cell_hdr;
        typedef gw_errmsg_hdr errmsg_hdr;
        typedef gw_evt_id evt_id;
        typedef gw_evt_hdr evt_hdr;

        void writeHeader ();
        void internal_write ( const void *data, size_t num_bytes );

        void write_event ( const evt_hdr * evt, size_t evt_size );

        struct int_stream
        {
            bool operator < ( const int_stream &s ) const;
            int_stream ( int table_id, const std :: string &column_name );

            int table_id;
            std :: string column_name;
        };

        std :: ofstream out;

        std :: string remote_db;
        std :: string schema_file_name;
        std :: string schema_db_spec;

        std :: map < std :: string, int > table_name_idx;
        std :: map < int_stream, int > column_name_idx;

        std :: vector < int_stream > streams;
        std :: vector < std :: string > table_names;

        uint64_t evt_count;
        uint64_t byte_count;

        int out_fd;
        bool isOpen;
        bool isClosed;
    };
}

#endif // _hpp_general_writer_
