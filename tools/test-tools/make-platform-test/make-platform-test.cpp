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

#include <stdlib.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <insdc/sra.h>

#include <string>

using namespace std;

#define CHECK_RC(CALL) do { auto && rc{CALL}; if (rc) return rc; } while(0)

static char const *VDB_INCDIR = nullptr;

static char const *platform_symbolic_names[] = { INSDC_SRA_PLATFORM_SYMBOLS };

static size_t constexpr NumberOfPlatforms() {
    return sizeof(platform_symbolic_names) / sizeof(platform_symbolic_names[0]);
}

static int NumberOfRows() {
    int const n = (int)NumberOfPlatforms();
    int i = 1;
    while (i <= n)
        i *= 2;
    return i;
}

static rc_t AddRow ( VCursor* p_curs, int64_t p_rowId, uint32_t p_colIdx, INSDC_SRA_platform_id value )
{
    CHECK_RC ( VCursorSetRowId ( p_curs, p_rowId ) );
    CHECK_RC ( VCursorOpenRow ( p_curs ) );
    CHECK_RC ( VCursorWrite ( p_curs, p_colIdx, sizeof(value) * 8, &value, 0, 1 ) );
    CHECK_RC ( VCursorCommitRow ( p_curs ) );
    CHECK_RC ( VCursorCloseRow ( p_curs ) );
    CHECK_RC ( VCursorCommit ( p_curs ) );
    return 0;
}

static rc_t Table()
{
    const string DefaultSchemaText  = R"###(
version 2;

include 'insdc/sra.vschema';

table Platform #1.0 {
    extern column < INSDC:SRA:platform_id > zip_encoding PLATFORM;
};
)###";

    VDBManager* mgr;
    CHECK_RC ( VDBManagerMakeUpdate ( & mgr, NULL ) );
    VSchema* schema;
    CHECK_RC ( VDBManagerMakeSchema ( mgr, & schema ) );
    if (VDB_INCDIR)
        VSchemaAddIncludePath(schema, "%s", VDB_INCDIR);
    CHECK_RC ( VSchemaParseText ( schema, NULL, DefaultSchemaText.c_str(), DefaultSchemaText.size() ) );

    VTable *tbl = nullptr;
    CHECK_RC ( VDBManagerCreateTable ( mgr
                                     , &tbl
                                     , schema
                                     , "Platform"
                                     , kcmInit + kcmMD5
                                     , "./input/platforms"
                                      ) );
    VSchemaRelease(schema);
    VDBManagerRelease(mgr);
    
    VCursor *curs = nullptr;
    CHECK_RC ( VTableCreateCursorWrite ( tbl, & curs, kcmInsert ) ) ;
    VTableRelease(tbl);

    uint32_t colId = 0;
    CHECK_RC ( VCursorAddColumn ( curs, &colId, "(" sra_platform_id_t ")PLATFORM" ) );
    CHECK_RC ( VCursorOpen ( curs ) );

    /// 0 is `SRA_PLATFORM_UNDEFINED`.
    /// current last defined is 26 `SRA_PLATFORM_HYK_GENE`.
    /// And we need a few more the that.
    int const n = NumberOfRows();
    for (auto i = 0; i < n; ++i) {
        CHECK_RC(AddRow(curs, ((int64_t)i) + 1, colId, (INSDC_SRA_platform_id)i));
    }
    VCursorRelease(curs);
    fprintf(stderr, "Wrote %i platform values + %i undefined.\n", (int)NumberOfPlatforms(), n - (int)NumberOfPlatforms());
    return 0;
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>

int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    VDB_INCDIR = (char const *)getenv("VDB_INCDIR");
    if (VDB_INCDIR)
        ;
    else
        VDB_INCDIR = argv[1];

    if (VDB_INCDIR)
        fprintf(stderr, "Will add include path '%s'.\n", VDB_INCDIR);
    else
        fprintf(stderr, "Will not add any include path.\n");

    return (int)Table();
}

}
