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

/**
 * Unit tests for Bio-end function
 */
#include "../../../tools/general-loader/general-writer.hpp"

using namespace ncbi;

const char* tableName = "table1";

void run( const char * p_caseId, bool inclusive, const int32_t coord_start[], const int32_t coord_len[], const int8_t coord_type[], int segments_number )
{
    std::string output_path = std::string ( "bio-end/input/" ) + p_caseId + ".gl";
    std::string db_path = std::string ( "bio-end/actual/" ) + p_caseId + "/db";
    std::string schema_path = inclusive ? "bio-end/bio-end-incl.vschema" : "bio-end/bio-end-excl.vschema";
    
    GeneralWriter *gw;
    try
    {
        gw = new GeneralWriter ( output_path );
        
        gw -> setRemotePath ( db_path );
        gw -> useSchema ( schema_path, "bio_end:test:database1" );

        int table_id = gw -> addTable ( tableName );
        int column_read_start_id = gw -> addIntegerColumn ( table_id, "out_read_start", 32 );
        int column_read_len_id = gw -> addIntegerColumn ( table_id, "out_read_len", 32 );
        int column_read_type_id = gw -> addIntegerColumn ( table_id, "out_read_type", 8 );
        
        gw -> open ();
        
        
#define write(column_id, column_bits, data) gw -> write ( column_id, column_bits, &data[0], segments_number );
        write ( column_read_start_id, 32, coord_start );
        write ( column_read_len_id, 32, coord_len );
        write ( column_read_type_id, 8, coord_type );
#undef write
        
        gw -> nextRow(table_id);
        
        gw -> endStream ();
        
        delete gw;
        
    }
    catch ( ... )
    {
        delete gw;
        throw;
    }
}

int main()
{
    {
        int32_t coord_start[]   = {0, 4, 8};
        int32_t coord_len[]     = {4, 4, 4};
        int8_t  coord_type[]    = {1, 0, 1};
        const char * test_case_id = "excl-1";
        bool inclusive = false;
        
        run( test_case_id, inclusive, coord_start, coord_len, coord_type, sizeof coord_type / sizeof coord_type[0] );
    }
    {
        int32_t coord_start[]   = {0, 4, 8};
        int32_t coord_len[]     = {4, 4, 4};
        int8_t  coord_type[]    = {0, 1, 0};
        const char * test_case_id = "excl-2";
        bool inclusive = false;
        
        run( test_case_id, inclusive, coord_start, coord_len, coord_type, sizeof coord_type / sizeof coord_type[0] );
    }
    {
        int32_t coord_start[]   = {0, 4, 8, 12, 12};
        int32_t coord_len[]     = {4, 4, 4, 0,  0};
        int8_t  coord_type[]    = {0, 1, 0, 1,  0};
        const char * test_case_id = "excl-3";
        bool inclusive = false;
        
        run( test_case_id, inclusive, coord_start, coord_len, coord_type, sizeof coord_type / sizeof coord_type[0] );
    }
    {
        int32_t coord_start[]   = {0, 4, 8, 12, 13};
        int32_t coord_len[]     = {4, 4, 4, 1,  0};
        int8_t  coord_type[]    = {0, 1, 0, 1,  0};
        const char * test_case_id = "excl-4";
        bool inclusive = false;
        
        run( test_case_id, inclusive, coord_start, coord_len, coord_type, sizeof coord_type / sizeof coord_type[0] );
    }
    {
        int32_t coord_start[]   = {0, 4, 8};
        int32_t coord_len[]     = {4, 4, 4};
        int8_t  coord_type[]    = {0, 1, 0};
        const char * test_case_id = "incl-1";
        bool inclusive = true;
        
        run( test_case_id, inclusive, coord_start, coord_len, coord_type, sizeof coord_type / sizeof coord_type[0] );
    }
    
    return 0;
}
