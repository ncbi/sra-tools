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

#include <ktst/unit_test.hpp>

using namespace std;

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/view.h>

TEST_SUITE ( VdbDumpViewAliasTestSuite );

TEST_CASE ( ListColumns )
{
    const VDBManager * mgr;
    REQUIRE_RC( VDBManagerMakeRead(&mgr, NULL) );

    const VDatabase *rdb;
    REQUIRE_RC( VDBManagerOpenDBRead ( mgr, &rdb, NULL, "./data/ViewDatabase" ) );

    const VView * view;
    REQUIRE_RC( VDatabaseOpenView ( rdb, & view, "VIEW1" ) );
    REQUIRE_NOT_NULL( view );

    struct KNamelist * names;
    REQUIRE_RC( VViewListCol ( view, & names ) );

    uint32_t count;
    REQUIRE_RC( KNamelistCount ( names, & count ) );
    // out of 3 columns only 2 have data (c3 comes from the empty TABLE3)
    REQUIRE_EQ( (uint32_t)2, count );

    REQUIRE( KNamelistContains( names, "c1" ) );
    REQUIRE( KNamelistContains( names, "c2" ) );

    KNamelistRelease( names );

    REQUIRE_RC( VViewRelease ( view ) );

    REQUIRE_RC( VDatabaseRelease ( rdb ) );
    REQUIRE_RC( VDBManagerRelease ( mgr ) );
}

//TODO: cover View1<View2 v>{ v2.c; (see TODO in prod-expr.c:VProdResolveMembExpr())}

//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>
#include <klib/debug.h>

int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
//KDbgSetString ( "VDB" );
    return VdbDumpViewAliasTestSuite(argc, argv);
}

}
