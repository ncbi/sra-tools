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
* Unit tests for vdb-dump's command line view spec parsing
*/

#include "vdb-dump-view-spec.h"

#include <ktst/unit_test.hpp>

#include <kfs/directory.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/view.h>


using namespace std;

const string ScratchDir = "./db/";

TEST_SUITE ( VdbDumpViewSpecTestSuite );

char error [ 1024 ] = { 0 };

TEST_CASE ( NullSelf )
{
    REQUIRE_RC_FAIL ( view_spec_parse ( "", NULL, error, sizeof ( error ) ) );
}

TEST_CASE ( NullSpec )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( NULL, & self, error, sizeof ( error ) ) );
    REQUIRE_NULL ( self );
    REQUIRE_EQ ( string ( "empty view specification" ), string ( error ) );
}

TEST_CASE ( EmptySpec )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "", & self, error, sizeof ( error ) ) );
    REQUIRE_NULL ( self );
    REQUIRE_EQ ( string ( "missing view name" ), string ( error ) );
}

TEST_CASE ( NullError )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<param>", & self, NULL, sizeof ( error ) ) );
    REQUIRE_NULL ( self );
}

TEST_CASE ( ZeroSizeError )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<param>", & self, error, 0 ) );
    REQUIRE_NULL ( self );
}

TEST_CASE ( MissingLeftAngle )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name", & self, error, sizeof ( error ) ) );
    REQUIRE_NULL ( self );
    REQUIRE_EQ ( string ( "missing '<' after the view name" ), string ( error ) );
}

TEST_CASE ( EmptyAngles )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<>", & self, error, sizeof ( error ) ) );
    REQUIRE_NULL ( self );
    REQUIRE_EQ ( string ( "missing view parameter(s)" ), string ( error ) );
}

TEST_CASE ( OneParam )
{
    view_spec * self;
    REQUIRE_RC ( view_spec_parse ( "name<param>", & self, error, sizeof ( error ) ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE ( string ( error ) . empty() );
    REQUIRE_EQ ( string ( "name" ), string ( self -> name ) );
    REQUIRE_EQ ( 1u, VectorLength ( & self -> args ) );
    REQUIRE_EQ ( string ( "param" ), string ( ( (const view_spec*) VectorGet ( & self -> args, 0 ) ) -> name ) );
    view_spec_free ( self );
}

TEST_CASE ( ManyParams )
{
    view_spec * self;
    REQUIRE_RC ( view_spec_parse ( "name<p0,p1, p2 ,p3    , \t  p4>", & self, error, sizeof ( error ) ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE ( string ( error ) . empty() );
    REQUIRE_EQ ( string ( "name" ), string ( self -> name ) );
    REQUIRE_EQ ( 5u, VectorLength ( & self -> args ) );
    REQUIRE_EQ ( string ( "p0" ), string ( ( (const view_spec*) VectorGet ( & self -> args, 0 ) ) -> name ) );
    REQUIRE_EQ ( string ( "p1" ), string ( ( (const view_spec*) VectorGet ( & self -> args, 1 ) ) -> name ) );
    REQUIRE_EQ ( string ( "p2" ), string ( ( (const view_spec*) VectorGet ( & self -> args, 2 ) ) -> name ) );
    REQUIRE_EQ ( string ( "p3" ), string ( ( (const view_spec*) VectorGet ( & self -> args, 3 ) ) -> name ) );
    REQUIRE_EQ ( string ( "p4" ), string ( ( (const view_spec*) VectorGet ( & self -> args, 4 ) ) -> name ) );
    REQUIRE_EQ ( string(), string(error) );
    view_spec_free ( self );
}

TEST_CASE ( MissingComma )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<p0 p1>", & self, error, sizeof ( error ) ) );
    REQUIRE_NULL ( self );
    REQUIRE_EQ ( string ( "expected ',' or '>' after a view parameter" ), string ( error ) );
}

TEST_CASE ( MissingParam )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<p0, >", & self, error, sizeof ( error ) ) );
    REQUIRE_NULL ( self );
    REQUIRE_EQ ( string ( "missing view parameter(s)" ), string ( error ) );
}

TEST_CASE ( ExtraChars )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<p>blah", & self, error, sizeof ( error ) ) );
    REQUIRE_NULL ( self );
    REQUIRE_EQ ( string ( "extra characters after '>'" ), string ( error ) );
}

TEST_CASE ( NestedViews )
{
    view_spec * self;

    REQUIRE_RC ( view_spec_parse ( "name<v1<n1,v12<n121,m122>>, t2, v3<t3>>", & self, error, sizeof ( error ) ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "name" ), string ( self -> name ) );
    REQUIRE_EQ ( 3u, VectorLength ( & self -> args ) );

    {
        const view_spec* v1 = (const view_spec*) VectorGet ( & self -> args, 0 );
        REQUIRE_EQ ( string ( "v1" ), string ( v1 -> name ) );
        REQUIRE_EQ ( (uint32_t)2, VectorLength ( & v1 -> args ) );

        const view_spec* n1 = (const view_spec*) VectorGet ( & v1 -> args, 0 );
        REQUIRE_EQ ( string ( "n1" ), string ( n1 -> name ) );
        REQUIRE_EQ ( (uint32_t)0, VectorLength ( & n1 -> args ) );

        const view_spec* v12 = (const view_spec*) VectorGet ( & v1 -> args, 1 );
        REQUIRE_EQ ( string ( "v12" ), string ( v12 -> name ) );
        REQUIRE_EQ ( (uint32_t)2, VectorLength ( & v12 -> args ) );
    }

    const view_spec* t2 = (const view_spec*) VectorGet ( & self -> args, 1 );
    REQUIRE_EQ ( string ( "t2" ), string ( t2 -> name ) );

    {
        const view_spec* v3 = (const view_spec*) VectorGet ( & self -> args, 2 );
        REQUIRE_EQ ( string ( "v3" ), string ( v3 -> name ) );
        REQUIRE_EQ ( (uint32_t)1, VectorLength ( & v3 -> args ) );
        const view_spec* t3 = (const view_spec*) VectorGet ( & v3 -> args, 0 );
        REQUIRE_EQ ( string ( "t3" ), string ( t3 -> name ) );
        REQUIRE_EQ ( (uint32_t)0, VectorLength ( & t3 -> args ) );
    }

    view_spec_free ( self );
}


const string SchemaText =
    "version 2;"
    "table T#1 { column ascii col; } "
    "view V#1 < T tbl > { column ascii c = tbl . col; }"
    "database D#1 { table T t; alias V<t> qq; } "
;

class ViewCursorFixture
{
public:
    ViewCursorFixture()
    :   m_spec ( 0 ),
        m_mgr ( 0 ),
        m_schema ( 0 ),
        m_db ( 0 ),
        m_view ( 0 ),
        m_cursor ( 0 )
    {
        KDirectory *dir;
        THROW_ON_RC ( KDirectoryNativeDir( & dir ) );
        THROW_ON_RC ( VDBManagerMakeUpdate( & m_mgr, dir ) );
        THROW_ON_RC ( VDBManagerMakeSchema( m_mgr, & m_schema ) );
        KDirectoryRelease ( dir );
    }
    ~ViewCursorFixture()
    {
        VCursorRelease ( m_cursor );
        VViewRelease ( m_view );
        VDatabaseRelease ( m_db );
        VSchemaRelease ( m_schema );
        VDBManagerRelease ( m_mgr );
        view_spec_free ( m_spec );
    }

    void MakeDatabase ( const string & p_dbName, const string & p_schemaText, const  string & p_schemaSpec )
    {
        KDirectory* wd;
        KDirectoryNativeDir ( & wd );
        KDirectoryRemove ( wd, true, p_dbName . c_str () );
        KDirectoryRelease ( wd );

        THROW_ON_RC ( VSchemaParseText ( m_schema, NULL, p_schemaText . c_str(), p_schemaText . size () ) );
        THROW_ON_RC ( VDBManagerCreateDB ( m_mgr,
                                          & m_db,
                                          m_schema,
                                          p_schemaSpec . c_str (),
                                          kcmInit + kcmMD5,
                                          "%s",
                                          p_dbName . c_str () ) );
    }

    void WriteRow ( VCursor * p_cursor, uint32_t p_colIdx, const std :: string & p_value )
    {
        THROW_ON_RC ( VCursorOpenRow ( p_cursor ) );
        THROW_ON_RC ( VCursorWrite ( p_cursor, p_colIdx, 8, p_value . c_str (), 0, p_value . length () ) );
        THROW_ON_RC ( VCursorCommitRow ( p_cursor ) );
        THROW_ON_RC ( VCursorCloseRow ( p_cursor ) );
    }

    void CreateDb ( const string & p_testName, const string p_schemaText = SchemaText )
    {
        const char * TableName = "t";
        const char * TableColumnName = "col";

        MakeDatabase ( ScratchDir + p_testName, p_schemaText, "D" );

        VTable* table;
        THROW_ON_RC ( VDatabaseCreateTable ( m_db, & table, TableName, kcmCreate | kcmMD5, "%s", TableName ) );
        VCursor * cursor;
        THROW_ON_RC ( VTableCreateCursorWrite ( table, & cursor, kcmInsert ) );
        THROW_ON_RC ( VTableRelease ( table ) );

        uint32_t column_idx;
        THROW_ON_RC ( VCursorAddColumn ( cursor, & column_idx, TableColumnName ) );
        THROW_ON_RC ( VCursorOpen ( cursor ) );

        // insert some rows
        WriteRow ( cursor, column_idx, "blah" );
        WriteRow ( cursor, column_idx, "eeee" );

        THROW_ON_RC ( VCursorCommit ( cursor ) );
        THROW_ON_RC ( VCursorRelease ( cursor ) );
    }

    view_spec *     m_spec;
    VDBManager *    m_mgr;
    VSchema *       m_schema;
    VDatabase *     m_db;
    const VView *   m_view;
    const VCursor * m_cursor;
};

FIXTURE_TEST_CASE ( ViewSpecOpen_NullSelf, ViewCursorFixture )
{
    CreateDb ( GetName () );

    REQUIRE_RC_FAIL ( view_spec_open ( NULL, m_db, m_schema, & m_view ) );
}

FIXTURE_TEST_CASE ( ViewSpecOpen_NullParam_Db, ViewCursorFixture )
{
    REQUIRE_RC ( view_spec_parse ( "name<p>", & m_spec, error, sizeof ( error ) ) );

    REQUIRE_RC_FAIL ( view_spec_open ( m_spec, NULL, m_schema, & m_view ) );
}

FIXTURE_TEST_CASE ( ViewSpecOpen_NullParam_Schema, ViewCursorFixture )
{
    CreateDb ( GetName () );
    REQUIRE_RC ( view_spec_parse ( "name<p>", & m_spec, error, sizeof ( error ) ) );

    REQUIRE_RC_FAIL ( view_spec_open ( m_spec, m_db, NULL, & m_view ) );
}

FIXTURE_TEST_CASE ( ViewSpecOpen_NullParam_View, ViewCursorFixture )
{
    CreateDb ( GetName () );
    REQUIRE_RC ( view_spec_parse ( "name<p>", & m_spec, error, sizeof ( error ) ) );

    REQUIRE_RC_FAIL ( view_spec_open ( m_spec, m_db, m_schema, NULL ) );
}

FIXTURE_TEST_CASE ( ViewSpecOpen_WrongNumberOfParams, ViewCursorFixture )
{
    CreateDb ( GetName () );
    REQUIRE_RC ( view_spec_parse ( "V<t, t>", & m_spec, error, sizeof ( error ) ) );

    REQUIRE_RC_FAIL ( view_spec_open ( m_spec, m_db, m_schema, & m_view ) );
}

FIXTURE_TEST_CASE ( ViewSpecOpen_ParamNotTable, ViewCursorFixture )
{
    CreateDb ( GetName () );
    REQUIRE_RC ( view_spec_parse ( "V<not_a_t>", & m_spec, error, sizeof ( error ) ) );

    REQUIRE_RC_FAIL ( view_spec_open ( m_spec, m_db, m_schema, & m_view ) );
}

//TODO: param not a view

FIXTURE_TEST_CASE ( MakeCursor, ViewCursorFixture )
{
    CreateDb ( GetName () );
    REQUIRE_RC ( view_spec_parse ( "V<t>", & m_spec, error, sizeof ( error ) ) );

    REQUIRE_RC ( view_spec_open ( m_spec, m_db, m_schema, & m_view ) );
    REQUIRE_NOT_NULL ( m_view );
    REQUIRE_RC ( VViewCreateCursor ( m_view, & m_cursor) );
    REQUIRE_NOT_NULL ( m_cursor );
    // walk the cursor
    uint32_t colIdx;
    REQUIRE_RC ( VCursorAddColumn ( m_cursor, & colIdx, "c" ) );
    REQUIRE_RC ( VCursorOpen ( m_cursor ) );
    char buf[1024];
    uint32_t rowLen;
    REQUIRE_RC ( VCursorReadDirect ( m_cursor, 1, colIdx, 8, buf, sizeof ( buf ), & rowLen ) );
    REQUIRE_EQ ( string ( "blah" ), string ( buf, rowLen ) );
    REQUIRE_RC ( VCursorReadDirect ( m_cursor, 2, colIdx, 8, buf, sizeof ( buf ), & rowLen ) );
    REQUIRE_EQ ( string ( "eeee") , string ( buf, rowLen ) );
}

FIXTURE_TEST_CASE ( NestedView, ViewCursorFixture )
{
    CreateDb ( GetName (), SchemaText + "view V1#1 < V v > { column ascii c1 = v . c; }" );
    REQUIRE_RC ( view_spec_parse ( "V1<V<t>>", & m_spec, error, sizeof ( error ) ) );

    REQUIRE_RC ( view_spec_open ( m_spec, m_db, m_schema, & m_view ) );
    REQUIRE_NOT_NULL ( m_view );
    REQUIRE_RC ( VViewCreateCursor ( m_view, & m_cursor) );
    REQUIRE_NOT_NULL ( m_cursor );
    // walk the cursor
    uint32_t colIdx;
    REQUIRE_RC ( VCursorAddColumn ( m_cursor, & colIdx, "c1" ) );
    REQUIRE_RC ( VCursorOpen ( m_cursor ) );
    char buf[1024];
    uint32_t rowLen;
    REQUIRE_RC ( VCursorReadDirect ( m_cursor, 1, colIdx, 8, buf, sizeof ( buf ), & rowLen ) );
    REQUIRE_EQ ( string ( "blah" ), string ( buf, rowLen ) );
    REQUIRE_RC ( VCursorReadDirect ( m_cursor, 2, colIdx, 8, buf, sizeof ( buf ), & rowLen ) );
    REQUIRE_EQ ( string ( "eeee") , string ( buf, rowLen ) );
}

//TODO: nested views v1<v2<t>,V3<t>>

//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>

int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return VdbDumpViewSpecTestSuite(argc, argv);
}

}
