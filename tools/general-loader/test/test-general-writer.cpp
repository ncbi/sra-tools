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

#include "../../tools/general-loader/general-writer.hpp"

#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace ncbi
{
    
    GeneralWriter * testCreateGw ( const char * out_path, const char * schema_path, const char * software_name, const char * version )
    {
        GeneralWriter * ret;
        if ( out_path == 0 )
        {
            // use stdout
            ret = new GeneralWriter ( 1 );
        }
        else
        {   
            // use a file
            ret = new GeneralWriter ( out_path );
        }
        
        ret -> setRemotePath ( schema_path );
        ret -> useSchema ( schema_path, std :: string ( "general_writer:test:db" ) );
        ret -> setSoftwareName ( software_name, version );
        
        return ret;
    }

    int testAddTable ( GeneralWriter *gw )
    {  
        return gw -> addTable ( std :: string ( "table1" ) );
    }

    void testAddColumn ( GeneralWriter *gw, int table_id, const char *column_names [], int column_count, int *stream_ids )
    {
        for ( int i = 0; i < column_count; ++ i )
        {
            const char *name = column_names [ i ];
            stream_ids [ i ] = gw -> addColumn ( table_id, name, 8, 0 ); // all columns are ascii for now
        }
    }

    void testDBAddDatabase ( GeneralWriter *gw, const char *mbr_name, const char *db_name, uint8_t create_mode )
    {
        gw -> dbAddDatabase ( 0, mbr_name, db_name, create_mode );
    }

    void testDBAddTable ( GeneralWriter *gw, const char *mbr_name, const char *table_name, uint8_t create_mode )
    {
        gw -> dbAddDatabase ( 0, mbr_name, table_name, create_mode );
    }

    void testOpen ( GeneralWriter *gw )
    {
        gw -> open ();
    }

    void testColumnDefault ( GeneralWriter *gw, int stream_id )
    {
        const char *data = "some string as data";
        gw -> columnDefault ( stream_id, 8, data, strlen ( data ) ); 
    }

    void testWrite ( GeneralWriter *gw, int table_id, int *stream_ids, int column_count, const char *file_names [] )
    {
        FILE **columns = ( FILE ** ) calloc ( column_count, sizeof *columns );
        if ( columns == NULL )
            throw "Failed to allocate memory";

        char buffer [ 4096 ];
        size_t buff_size = sizeof buffer;

        // Populate array of file pointers
        for ( int i = 0; i < column_count; ++ i )
        { 
            const char *name = file_names [ i ];
            FILE *column = fopen ( name, "r" );
            
            if ( column == NULL )
                throw "Error opening file";
            
            columns [ i ] = column;
            
            column = 0;
        }
        
        // open the stream
        gw -> open ();
        
        // set default values for each column
        for ( int i = 0; i < column_count; ++ i )
            gw -> columnDefault ( stream_ids [ i ], 8, "EOF", strlen ( "EOF" ) );

        while ( 1 )
        {
            bool validColumn = false;
            // write one line from each column
            for ( int i = 0; i < column_count; ++ i )
            {
                // Skip any NULL entries in the FILE array
                if ( columns [ i ] == NULL )
                    continue;
                
                FILE *column = columns [ i ];
                
                if ( fgets ( buffer, buff_size, column ) ==  NULL )
                {
                    if ( ferror ( column ) )
                    {
                        fclose ( column );
                        throw "Error reading from file";
                    }
                    
                    // Found EOF while reading cell data
                    // close FILE and set array pointer to NULL
                    fclose ( column );
                    column = 0;
                    columns [ i ] = 0;
                    continue;
                }
                
                // found valid data, write line and go to next column
                int elem_count = strlen ( ( const char * ) buffer );

                // ensure there are no new lines at the end of the string
                for ( int j = elem_count - 1; j >= 0; -- j )
                {
                    if ( buffer [ j ] == '\n' )
                        -- elem_count;
                }

                gw -> write ( stream_ids [ i ], 8, buffer, elem_count );
                validColumn = true;
            }
            
            if ( ! validColumn )
                break;

            // go to the next row
            gw -> nextRow ( table_id );
        }
        free ( columns );
    }

    void testAddDBMetadataNode ( GeneralWriter *gw, const char * node, const char *value )
    {
        gw -> setDBMetadataNode ( 0, node, value );
    }

    void testAddTblMetadataNode ( GeneralWriter *gw, const char * node, const char *value )
    {
        gw -> setTblMetadataNode ( 1, node, value );
    }

    void testAddColMetadataNode ( GeneralWriter *gw, const char * node, const char *value )
    {
        gw -> setColMetadataNode ( 1, node, value );
    }

    void testProgMsg ( GeneralWriter *gw, const char *name, 
                       uint32_t version, uint64_t done, uint64_t total )
    {
        gw -> progMsg ( name, version, done, total );
    }

    void testEndStream ( GeneralWriter *gw )
    {
        gw -> endStream ();
        
        try
        {
            std :: cerr << "Attempting add table after endStream" << std :: endl;            
            gw -> addTable ( "table1" );
        }
        catch ( const char x [] )
        {
            std :: cerr << x << std :: endl;
            std :: cerr << "addTable correctly failed" << std :: endl;
        }
    }

    void runTest ( int column_count, const char * columns [], const char *outfile, const char * schema_path )
    {
        GeneralWriter *gw;
        try
        {
            const char * column_names [ column_count ];

            for ( int i = 0; i < column_count ; ++ i )
                column_names [ i ] = columns [ i ];

            gw = testCreateGw ( outfile, schema_path, "softwarename", "2" );
            std :: cerr << "CreateGw Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;
                        
            int table_id = testAddTable ( gw );
            std :: cerr << "addTable Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;

            int stream_ids [ column_count ];
            testAddColumn ( gw, table_id, column_names, column_count, stream_ids ); 
            std :: cerr << "addColumn Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;

            testDBAddDatabase ( gw, "member_name", "db_name", 1 );
            std :: cerr << "dbAddDatabase Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;

            testDBAddTable ( gw, "member_name", "table_name", 1 );
            std :: cerr << "dbAddTable Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;

            testWrite ( gw, table_id, stream_ids, column_count, column_names );
            std :: cerr << "write Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;

            testAddDBMetadataNode ( gw, "db_metadata_node", "01a2b3c4d" );
            std :: cerr << "setDBMetadataNode Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;
            testAddTblMetadataNode ( gw, "tbl_metadata_node", "01a2b3c4d" );
            std :: cerr << "setTblMetadataNode Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;
            testAddColMetadataNode ( gw, "col_metadata_node", "01a2b3c4d" );
            std :: cerr << "setColMetadataNode Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;

            testProgMsg ( gw, "name", 1, 54768, 64000 );
            std :: cerr << "setProgMsg Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;

            testEndStream ( gw );
            std :: cerr << "endStream Success" << std :: endl;
            std :: cerr << "---------------------------------" << std :: endl;

        }
        catch ( ... )
        {
            delete gw;
            throw;
        }
        delete gw;
    }
}

const char * getArg ( const char*  &arg, int & i, int argc, char * argv [] )
{
    if ( arg [ 1 ] != 0 )
    {
        const char * next = arg + 1;
        arg = "\0";
        return next;
    }
    
    if ( ++ i == argc )
        throw "Missing argument";
    
    return argv [ i ];
}

int main ( int argc, char * argv [] )
{
    int status = 1;
    
    try
    {

        const char *outfile = "./db/";
        const char *schema_path = "./test-general-writer.vschema";
        int num_columns = 0;
    
        for ( int i = 1; i < argc; ++ i )
        {
            const char * arg = argv [ i ];
            if ( arg [ 0 ] != '-' )
            {
                // have an input column
                argv [ num_columns ++ ] = ( char* ) arg;
            }
            else do switch ( ( ++ arg ) [ 0 ] )
            {
                case 'o':
                    outfile = getArg ( arg, i, argc, argv );
                    break;
                case 's':
                    schema_path = getArg ( arg, i, argc, argv );
                default:
                    throw "Invalid argument";
            }
            while ( arg [ 1 ] != 0 );
        }
        
        if ( num_columns == 0 )
        {
            const char * columns [ 2 ] = { "input/column01", "input/column02" };
            ncbi :: runTest ( 2, columns, outfile, schema_path );
        }
        else
        {
            ncbi :: runTest ( num_columns, ( const char ** ) argv, outfile, schema_path );
        }
        
        status = 0;
    }
    catch ( const char x [] )
    {
        std :: cerr << x << std :: endl;
    }
    
    return status;
}
