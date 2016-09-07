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
* Create test databases for vdb-dump
*/

#include <fstream>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

using namespace std;

#define CHECK_RC(call) { rc_t rc = call; if ( rc != 0 ) return rc; }

rc_t
AddRow ( VCursor* p_curs, int64_t p_rowId, uint32_t p_colIdx, const string& p_value )
{
    CHECK_RC ( VCursorSetRowId ( p_curs, p_rowId ) );
    CHECK_RC ( VCursorOpenRow ( p_curs ) );
    CHECK_RC ( VCursorWrite ( p_curs, p_colIdx, 8, p_value.c_str(), 0, p_value.size() ) );
    CHECK_RC ( VCursorCommitRow ( p_curs ) );
    CHECK_RC ( VCursorCloseRow ( p_curs ) );
    CHECK_RC ( VCursorCommit ( p_curs ) );
    return 0;
}

rc_t
NestedDatabase()
{
    const string ScratchDir         = "./data/";
    const string DefaultSchemaText  =
        "table table1 #1.0.0 { column ascii col; };\n"
        "table table2 #1.0.0 { column ascii col; };\n"

        "database database1_1 #1 { table table1 #1 TABLE1; };\n"
        "database database1_2 #1 { table table2 #1 TABLE2; };\n"

        "database database1 #1\n"
        "{\n"
        " database database1_1 #1 SUBSUBDB_1;\n"
        " database database1_2 #1 SUBSUBDB_2;\n"
        " table table1 #1 TABLE1;\n"
        "};\n"

        "database database2 #1\n"
        "{\n"
        " database database1_1 #1 SUBSUBDB_1;\n"
        " database database1_2 #1 SUBSUBDB_2;\n"
        " table table2 #1 TABLE2;\n"
        "};\n"

        "database root_database #1\n"
        "{\n"
        " database database1 #1 SUBDB_1;\n"
        " database database2 #1 SUBDB_2;\n"
        " table table1 #1 TABLE1;\n"
        " table table2 #1 TABLE2;\n"
        " };\n"
    ;
    const string DefaultDatabase    = "root_database";

    VDBManager* mgr;
    CHECK_RC ( VDBManagerMakeUpdate ( & mgr, NULL ) );
    VSchema* schema;
    CHECK_RC ( VDBManagerMakeSchema ( mgr, & schema ) );
    CHECK_RC ( VSchemaParseText ( schema, NULL, DefaultSchemaText.c_str(), DefaultSchemaText.size() ) );

    VDatabase* db;
    CHECK_RC ( VDBManagerCreateDB ( mgr,
                                    & db,
                                    schema,
                                    DefaultDatabase . c_str(),
                                    kcmInit + kcmMD5,
                                    "%s",
                                    ( ScratchDir + "NestedDatabase" ) . c_str() ) );

    {   // SUBDB_1
        VDatabase *db1;
        CHECK_RC ( VDatabaseCreateDB ( db, & db1, "SUBDB_1", kcmInit + kcmMD5, "SUBDB_1" ) );
        {   // SUBSUBDB_1
            VDatabase *db1_1;
            CHECK_RC ( VDatabaseCreateDB ( db1, & db1_1, "SUBSUBDB_1", kcmInit + kcmMD5, "SUBSUBDB_1" ) );
            {   // TABLE1
                VTable *tab;
                CHECK_RC ( VDatabaseCreateTable ( db1_1, & tab, "TABLE1", kcmInit + kcmMD5, "TABLE1" ) );
                VCursor *curs;
                CHECK_RC ( VTableCreateCursorWrite ( tab, & curs, kcmInsert ) ) ;
                uint32_t idx;
                CHECK_RC ( VCursorAddColumn ( curs, & idx, "col" ) );
                CHECK_RC ( VCursorOpen ( curs ) );
                CHECK_RC ( AddRow ( curs, 1, idx, "1-1/1" ) );
                CHECK_RC ( AddRow ( curs, 2, idx, "1-1/2" ) );
                CHECK_RC ( VCursorRelease ( curs ) );
                CHECK_RC ( VTableRelease ( tab ) );
            }
            CHECK_RC ( VDatabaseRelease ( db1_1 ) );
        }
        {   // SUBSUBDB_2
            VDatabase *db1_2;
            CHECK_RC ( VDatabaseCreateDB ( db1, & db1_2, "SUBSUBDB_2", kcmInit + kcmMD5, "SUBSUBDB_2" ) );
            {   // TABLE2
                VTable *tab;
                CHECK_RC ( VDatabaseCreateTable ( db1_2, & tab, "TABLE2", kcmInit + kcmMD5, "TABLE2" ) );
                VCursor *curs;
                CHECK_RC ( VTableCreateCursorWrite ( tab, & curs, kcmInsert ) ) ;
                uint32_t idx;
                CHECK_RC ( VCursorAddColumn ( curs, & idx, "col" ) );
                CHECK_RC ( VCursorOpen ( curs ) );
                CHECK_RC ( AddRow ( curs, 1, idx, "2-2/1" ) );
                CHECK_RC ( AddRow ( curs, 2, idx, "2-2/2" ) );
                CHECK_RC ( VCursorRelease ( curs ) );
                CHECK_RC ( VTableRelease ( tab ) );
            }
            CHECK_RC ( VDatabaseRelease ( db1_2 ) );
        }
        {   // TABLE1
            VTable *tab;
            CHECK_RC ( VDatabaseCreateTable ( db1, & tab, "TABLE1", kcmInit + kcmMD5, "TABLE1" ) );
            CHECK_RC ( VTableRelease ( tab ) );
        }
        CHECK_RC ( VDatabaseRelease ( db1 ) );
    }
    {   // SUBDB_2
        VDatabase *db2;
        CHECK_RC ( VDatabaseCreateDB ( db, & db2, "SUBDB_2", kcmInit + kcmMD5, "SUBDB_2" ) );
        {   // SUBSUBDB_1
            VDatabase *db2_1;
            CHECK_RC ( VDatabaseCreateDB ( db2, & db2_1, "SUBSUBDB_1", kcmInit + kcmMD5, "SUBSUBDB_1" ) );
            {   // TABLE1
                VTable *tab;
                CHECK_RC ( VDatabaseCreateTable ( db2_1, & tab, "TABLE1", kcmInit + kcmMD5, "TABLE1" ) );
                CHECK_RC ( VTableRelease ( tab ) );
            }
            CHECK_RC ( VDatabaseRelease ( db2_1 ) );
        }
        {   // SUBSUBDB_2
            VDatabase *db2_2;
            CHECK_RC ( VDatabaseCreateDB ( db2, & db2_2, "SUBSUBDB_2", kcmInit + kcmMD5, "SUBSUBDB_2" ) );
            {   // TABLE2
                VTable *tab;
                CHECK_RC ( VDatabaseCreateTable ( db2_2, & tab, "TABLE2", kcmInit + kcmMD5, "TABLE2" ) );
                CHECK_RC ( VTableRelease ( tab ) );
            }
            CHECK_RC ( VDatabaseRelease ( db2_2 ) );
        }
        {   // TABLE2
            VTable *tab;
            CHECK_RC ( VDatabaseCreateTable ( db2, & tab, "TABLE2", kcmInit + kcmMD5, "TABLE2" ) );
            CHECK_RC ( VTableRelease ( tab ) );
        }
        CHECK_RC ( VDatabaseRelease ( db2 ) );
    }

    {   // TABLE1
        VTable *tab;
        CHECK_RC ( VDatabaseCreateTable ( db, & tab, "TABLE1", kcmInit + kcmMD5, "TABLE1" ) );
        CHECK_RC ( VTableRelease ( tab ) );
    }
    {   // TABLE2
        VTable *tab;
        CHECK_RC ( VDatabaseCreateTable ( db, & tab, "TABLE2", kcmInit + kcmMD5, "TABLE2" ) );
        CHECK_RC ( VTableRelease ( tab ) );
    }

    CHECK_RC ( VSchemaRelease ( schema ) );
    CHECK_RC ( VDatabaseRelease ( db ) );
    CHECK_RC ( VDBManagerRelease ( mgr ) );
    return 0;
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kapp/args.h>
#include <kfg/config.h>

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}

rc_t CC Usage ( const Args * args )
{
    return 0;
}

const char UsageDefaultName[] = "makedb";

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();

    return NestedDatabase();
}

}
