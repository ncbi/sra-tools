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
 * Unit tests for text index functionality
 */
#include "../../../tools/general-loader/general-writer.hpp"

#include <string>

#define TEST_SUITE "idx-text"

using namespace ncbi;

const char* tableName = "table1";

enum TestCaseType
{
    caseInsensitiveLower,
    caseInsensitiveUpper,
    caseSensitive,
    defaultCaseSensitive
};

void run( const char * p_caseId, TestCaseType test_case_type, const std::string names[], size_t names_len )
{
    std::string output_path = std::string ( TEST_SUITE "/input/" ) + p_caseId + ".gl";
    std::string db_path = std::string ( TEST_SUITE "/actual/" ) + p_caseId + "/db";
    std::string schema_path = TEST_SUITE "/idx-text.vschema";
    
    GeneralWriter *gw;
    try
    {
        const char* db_schema;
        switch (test_case_type) {
            case caseInsensitiveLower:
                db_schema = "idx_text:ci_lower:test:database";
                break;
            case caseInsensitiveUpper:
                db_schema = "idx_text:ci_upper:test:database";
                break;
            case caseSensitive:
                db_schema = "idx_text:cs:test:database";
                break;
            case defaultCaseSensitive:
                db_schema = "idx_text:default_cs:test:database";
                break;
            default:
                assert(false);
        }
        gw = new GeneralWriter ( output_path );
        
        gw -> setRemotePath ( db_path );
        gw -> useSchema ( schema_path, db_schema );
        
        int table_id = gw -> addTable ( tableName );
        int column_name_id = gw -> addColumn ( table_id, "NAME", 8 );
        
        gw -> open ();
        
        for (int i = 0; i < names_len; ++i)
        {
            gw -> write ( column_name_id, 8, names[i].c_str(), names[i].size() );
            
            gw -> nextRow(table_id);
        }
        
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
        std::string names[]   = {"a", "a", "a", "A", "b"};
        const char * test_case_id = "ci-1";
        
        run( test_case_id, caseInsensitiveLower, names, sizeof names / sizeof names[0] );
    }

    {
        std::string names[]   = {"abcdefghi", "abcdefghi", "abcdefghi", "abcdefGHI", "ABcdefghi", "ABcdefghi1234567890"};
        const char * test_case_id = "ci-2";
        
        run( test_case_id, caseInsensitiveLower, names, sizeof names / sizeof names[0] );
    }
    
    {
        std::string names[]   = {"abcdefghi", "abcdefghi", "abcdefGHI", "ABcdefghi"};
        const char * test_case_id = "ci-3";
        
        run( test_case_id, caseInsensitiveLower, names, sizeof names / sizeof names[0] );
    }
    
    {
        std::string names[]   = {"abcdefghi", "abcdefghi", "abcdefGHI", "ABcdefghi"};
        const char * test_case_id = "ci-4";
        
        run( test_case_id, caseInsensitiveUpper, names, sizeof names / sizeof names[0] );
    }
    
    {
        std::string names[]   = {"abcdefghi", "abcdefghi", "abcdefghi", "abcdefGHI", "ABcdefghi", "ABcdefghi1234567890"};
        const char * test_case_id = "cs-1";
        
        run( test_case_id, caseSensitive, names, sizeof names / sizeof names[0] );
    }
    
    {
        std::string names[]   = {"abcdefghi", "abcdefghi", "abcdefghi", "abcdefGHI", "ABcdefghi", "ABcdefghi1234567890"};
        const char * test_case_id = "cs-2";
        
        run( test_case_id, defaultCaseSensitive, names, sizeof names / sizeof names[0] );
    }
    
    return 0;
}
