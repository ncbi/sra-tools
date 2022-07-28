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
 * Perform checks of function idx:text:lookup on created databases,
 * because it requires passing parameter through cursor parameter QUERY_NAME
 */

#include <klib/log.h>
#include <klib/writer.h>
#include <kfs/directory.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>

#include <iostream>
#include <map>

#define TEST_SUITE "idx-text"
#define TABLE_NAME "table1"

struct RowRange {
    int64_t start_id;
    int64_t stop_id;
};

struct TestCase {
    const char* path;
    std::map<std::string, RowRange> key_ranges;
};

static std::map<std::string, TestCase> test_cases;
static const char* test_case_name;

rc_t runChecks(const TestCase& test_case, const VCursor * cursor, uint32_t name_idx, uint32_t name_range_idx)
{
    rc_t rc;
    int64_t first_id;
    uint64_t count_id;
    
    rc = VCursorIdRange( cursor, name_idx, &first_id, &count_id );
    if (rc != 0)
    {
        LOGERR( klogInt, rc, "VCursorIdRange() failed" );
        return rc;
    }
    
    for (uint64_t row_id = first_id; row_id < first_id + count_id; ++row_id)
    {
        const char * name = NULL;
        uint32_t name_len;
        RowRange *row_range;
        
        rc = VCursorCellDataDirect( cursor, row_id, name_idx, NULL, (void const **)&name, NULL, &name_len );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "VCursorCellDataDirect() failed" );
            return rc;
        }
        
        if ( name_len == 0 )
            continue;

        rc = VCursorParamsSet( ( struct VCursorParams const * )cursor, "QUERY_NAME", "%.*s", name_len, name );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "VCursorParamsSet() failed" );
            return rc;
        }
        
        rc = VCursorCellDataDirect( cursor, row_id, name_range_idx, NULL, (void const **)&row_range, NULL, NULL );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "VCursorCellDataDirect() failed" );
            return rc;
        }
        
        std::string name_str(name, name_len);
        
        if (test_case.key_ranges.find(name_str) == test_case.key_ranges.end())
        {
            PLOGMSG( klogInt, (klogErr, "Unexpected name '$(NAME)' in test case '$(TC_NAME)'", "TC_NAME=%s,NAME=%s", test_case_name, name_str.c_str()) );
            return 1;
        }
        
        RowRange row_range_exp = test_case.key_ranges.find(name_str)->second;
        if (row_range->start_id != row_range_exp.start_id || row_range->stop_id != row_range_exp.stop_id)
        {
            PLOGMSG( klogInt, (klogErr, "Row range for name '$(NAME)' in test case '$(TC_NAME)' does not match. Expected: $(EXP_S)-$(EXP_F), actual: $(ACT_S)-$(ACT_F)",
                               "TC_NAME=%s,NAME=%s,EXP_S=%ld,EXP_F=%ld,ACT_S=%ld,ACT_F=%ld",
                               test_case_name, name_str.c_str(), row_range_exp.start_id, row_range_exp.stop_id, row_range->start_id, row_range->stop_id) );
            return 1;
        }
    }
    
    return rc;
}

rc_t run(const TestCase& test_case)
{
    rc_t rc;
    KDirectory * cur_dir;
    const VDBManager * manager;
    const VDatabase * database;
    const VTable * table;
    const VCursor * cursor;
    uint32_t name_idx;
    uint32_t name_range_idx;
    
    rc = KDirectoryNativeDir( &cur_dir );
    if ( rc != 0 )
        LOGERR( klogInt, rc, "KDirectoryNativeDir() failed" );
    else
    {
        rc = VDBManagerMakeRead ( &manager, cur_dir );
        if ( rc != 0 )
            LOGERR( klogInt, rc, "VDBManagerMakeRead() failed" );
        else
        {
            rc = VDBManagerOpenDBRead( manager, &database, NULL, "%s", test_case.path );
            if (rc != 0)
                LOGERR( klogInt, rc, "VDBManagerOpenDBRead() failed" );
            else
            {
                rc = VDatabaseOpenTableRead( database, &table, "%s", TABLE_NAME );
                if ( rc != 0 )
                    LOGERR( klogInt, rc, "VDatabaseOpenTableRead() failed" );
                else
                {
                    rc = VTableCreateCursorRead( table, &cursor );
                    if ( rc != 0 )
                        LOGERR( klogInt, rc, "VTableCreateCursorRead() failed" );
                    else
                    {
                        /* add columns to cursor */
                        rc = VCursorAddColumn( cursor, &name_idx, "(utf8)NAME" );
                        if ( rc != 0 )
                            LOGERR( klogInt, rc, "VCursorAddColumn() failed" );
                        else
                        {
                            rc = VCursorAddColumn( cursor, &name_range_idx, "NAME_RANGE" );
                            if ( rc != 0 )
                                LOGERR( klogInt, rc, "VCursorAddColumn() failed" );
                            else
                            {
                                rc = VCursorOpen( cursor );
                                if (rc != 0)
                                    LOGERR( klogInt, rc, "VCursorOpen() failed" );
                                else
                                    rc = runChecks( test_case, cursor, name_idx, name_range_idx );
                            }
                        }
                        VCursorRelease( cursor );
                    }
                    VTableRelease( table );
                }
                VDatabaseRelease( database );
            }
            
            VDBManagerRelease( manager );
        }
        KDirectoryRelease( cur_dir );
    }
    return rc;
}

/* New test cases should go here */
void initTestCases()
{
    {
        std::map<std::string, RowRange> key_ranges;
        {
            RowRange range = {2, 5};
            key_ranges["a"] = range;
            key_ranges["A"] = range;
        }
        {
            RowRange range = {7, 7};
            key_ranges["b"] = range;
        }
        
        TestCase tc = { TEST_SUITE "/actual/ci-1/db", key_ranges };
        test_cases["ci-1"] = tc;
    }
    
    {
        std::map<std::string, RowRange> key_ranges;
        {
            RowRange range = {1, 5};
            key_ranges["abcdefghi"] = range;
            key_ranges["abcdefGHI"] = range;
            key_ranges["ABcdefghi"] = range;
        }
        {
            RowRange range = {6, 6};
            key_ranges["ABcdefghi1234567890"] = range;
        }
        
        TestCase tc = { TEST_SUITE "/actual/ci-2/db", key_ranges };
        test_cases["ci-2"] = tc;
    }
    
    {
        std::map<std::string, RowRange> key_ranges;
        {
            RowRange range = {1, 4};
            key_ranges["abcdefghi"] = range;
            key_ranges["abcdefGHI"] = range;
            key_ranges["ABcdefghi"] = range;
        }
        
        TestCase tc = { TEST_SUITE "/actual/ci-3/db", key_ranges };
        test_cases["ci-3"] = tc;
    }
    
    {
        std::map<std::string, RowRange> key_ranges;
        {
            RowRange range = {1, 4};
            key_ranges["abcdefghi"] = range;
            key_ranges["abcdefGHI"] = range;
            key_ranges["ABcdefghi"] = range;
        }
        
        TestCase tc = { TEST_SUITE "/actual/ci-4/db", key_ranges };
        test_cases["ci-4"] = tc;
    }
    
    {
        std::map<std::string, RowRange> key_ranges;
        {
            RowRange range = {1, 3};
            key_ranges["abcdefghi"] = range;
        }
        
        {
            RowRange range = {4, 4};
            key_ranges["abcdefGHI"] = range;
        }
        
        {
            RowRange range = {5, 5};
            key_ranges["ABcdefghi"] = range;
        }
        
        {
            RowRange range = {6, 6};
            key_ranges["ABcdefghi1234567890"] = range;
        }
        
        TestCase tc = { TEST_SUITE "/actual/cs-1/db", key_ranges };
        test_cases["cs-1"] = tc;
    }
    
    {
        std::map<std::string, RowRange> key_ranges;
        {
            RowRange range = {1, 3};
            key_ranges["abcdefghi"] = range;
        }
        
        {
            RowRange range = {4, 4};
            key_ranges["abcdefGHI"] = range;
        }
        
        {
            RowRange range = {5, 5};
            key_ranges["ABcdefghi"] = range;
        }
        
        {
            RowRange range = {6, 6};
            key_ranges["ABcdefghi1234567890"] = range;
        }
        
        TestCase tc = { TEST_SUITE "/actual/cs-2/db", key_ranges };
        test_cases["cs-2"] = tc;
    }
}

int main(int argc, const char* argv[])
{
    /* init logging */
    KLogLibHandlerSetStdErr();
    KLogHandlerSetStdErr();
    KWrtInit("checklookup", 0);
    
    initTestCases();
    
    if (argc < 2)
    {
        std::cerr << "Please specify test case name" << std::endl;
        return 1;
    }
    
    test_case_name = argv[1];
    if (test_cases.find(test_case_name) == test_cases.end())
    {
        std::cerr << "Unknown test case name: " << test_case_name << std::endl;
        return 2;
    }
    
    rc_t rc = run(test_cases[test_case_name]);
    if (rc != 0)
        return 3;
    
    std::cout << "Success" << std::endl;
    return 0;
}
