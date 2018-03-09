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

#include "../../tools/vdb-dump/vdb-dump-view-spec.c"

#include <ktst/unit_test.hpp>

using namespace std;

TEST_SUITE ( VdbDumpViewSpecTestSuite );

TEST_CASE ( NullSelf )
{
    REQUIRE_RC_FAIL ( view_spec_parse ( "", NULL ) );
}

TEST_CASE ( NullSpec )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( NULL, & self ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "empty view specification" ), string ( self -> error ) );
    view_spec_free ( self );
}

TEST_CASE ( EmptySpec )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "", & self ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "missing view name" ), string ( self -> error ) );
    view_spec_free ( self );
}

TEST_CASE ( MissingLeftAngle )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name", & self ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "missing '<' after the view name" ), string ( self -> error ) );
    view_spec_free ( self );
}

TEST_CASE ( EmptyAngles )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<>", & self ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "missing view parameter(s)" ), string ( self -> error ) );
    view_spec_free ( self );
}

TEST_CASE ( OneParam )
{
    view_spec * self;
    REQUIRE_RC ( view_spec_parse ( "name<param>", & self ) );
    REQUIRE_EQ ( 0, (int)self -> error [ 0 ] );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "name" ), string ( self -> view_name . addr, self -> view_name . len ));
    REQUIRE_EQ ( 1u, VectorLength ( & self -> args ) );
    REQUIRE_EQ ( string ( "param" ), string ( (const char*) VectorGet ( & self -> args, 0 ) ) );
    view_spec_free ( self );
}

TEST_CASE ( ManyParams )
{
    view_spec * self;
    REQUIRE_RC ( view_spec_parse ( "name<p0,p1, p2 ,p3    , \t  p4>", & self ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( 0, (int)self -> error [ 0 ] );
    REQUIRE_EQ ( string ( "name" ), string ( self -> view_name . addr, self -> view_name . len ));
    REQUIRE_EQ ( 5u, VectorLength ( & self -> args ) );
    REQUIRE_EQ ( string ( "p0" ), string ( (const char*) VectorGet ( & self -> args, 0 ) ) );
    REQUIRE_EQ ( string ( "p1" ), string ( (const char*) VectorGet ( & self -> args, 1 ) ) );
    REQUIRE_EQ ( string ( "p2" ), string ( (const char*) VectorGet ( & self -> args, 2 ) ) );
    REQUIRE_EQ ( string ( "p3" ), string ( (const char*) VectorGet ( & self -> args, 3 ) ) );
    REQUIRE_EQ ( string ( "p4" ), string ( (const char*) VectorGet ( & self -> args, 4 ) ) );
    REQUIRE_EQ ( 0, (int)self -> error [ 0 ] );
    view_spec_free ( self );
}

TEST_CASE ( MissingComma )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<p0 p1>", & self ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "expected ',' or '>' after a view parameter" ), string ( self -> error ) );
    view_spec_free ( self );
}

TEST_CASE ( MissingParam )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<p0, >", & self ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "missing view parameter(s) after ','" ), string ( self -> error ) );
    view_spec_free ( self );
}

TEST_CASE ( ExtraChars )
{
    view_spec * self;
    REQUIRE_RC_FAIL ( view_spec_parse ( "name<p>blah", & self ) );
    REQUIRE_NOT_NULL ( self );
    REQUIRE_EQ ( string ( "extra characters after '>'" ), string ( self -> error ) );
    view_spec_free ( self );
}

TEST_CASE ( CreateViewCursor_NullSelf )
{
    VCursor * cur;
    REQUIRE_RC_FAIL ( CreateViewCursor ( NULL, & cur ) );
}

TEST_CASE ( CreateViewCursor_NullParam )
{
    view_spec * self;
    REQUIRE_RC ( view_spec_parse ( "name<p>", & self ) );
    REQUIRE_RC_FAIL ( CreateViewCursor ( self, NULL ) );
}

//TODO: nested views v1<v2<t>,V3<t>>

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

const char UsageDefaultName[] = "test-view-spec";

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    rc_t rc=VdbDumpViewSpecTestSuite(argc, argv);
    return rc;
}

}
