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
* Unit tests for mapping between fragment Id and fragment name
*/

#include <ktst/unit_test.hpp>

#include "../../tools/loaders/fastq-loader/id2name.h"

using namespace std;

TEST_SUITE(Id2NameSuite);

TEST_CASE(Init_NullSelf)
{
    REQUIRE_RC_FAIL ( Id2Name_Init ( NULL ) );
}

TEST_CASE(Whack_NullSelf)
{
    REQUIRE_RC_FAIL ( Id2Name_Whack ( NULL ) );
}

TEST_CASE(Init_Whack)
{
    Id2name self;
    REQUIRE_RC ( Id2Name_Init ( & self ) );
    REQUIRE_RC ( Id2Name_Whack ( & self ) );
}

class Id2name_Fixture
{
public:
    Id2name_Fixture()
    {
        Id2Name_Init ( & m_self );
    }
    ~Id2name_Fixture()
    {
        Id2Name_Whack ( & m_self );
    }

    Id2name         m_self;
    const char *    m_res;
};

FIXTURE_TEST_CASE ( Add_NullSelf, Id2name_Fixture )
{
    REQUIRE_RC_FAIL ( Id2Name_Add( NULL, 1, "name" ) );
}

FIXTURE_TEST_CASE ( Add_NullName, Id2name_Fixture )
{
    REQUIRE_RC_FAIL ( Id2Name_Add( & m_self, 1, NULL ) );
}

FIXTURE_TEST_CASE ( Get_NullSelf, Id2name_Fixture )
{
    REQUIRE_RC_FAIL ( Id2Name_Get ( NULL, 1, & m_res ) );
}

FIXTURE_TEST_CASE ( Get_NullRes, Id2name_Fixture )
{
    REQUIRE_RC_FAIL ( Id2Name_Get ( & m_self, 1, NULL ) );
}

FIXTURE_TEST_CASE ( Get_Empty, Id2name_Fixture )
{
    REQUIRE_RC_FAIL ( Id2Name_Get ( & m_self, 1, & m_res ) );
}

FIXTURE_TEST_CASE ( Get_Found, Id2name_Fixture )
{
    REQUIRE_RC ( Id2Name_Add( & m_self, 1, "name" ) );
    REQUIRE_RC ( Id2Name_Get ( & m_self, 1, & m_res ) );
    REQUIRE_EQ ( string ( "name" ), string ( m_res ) );
}
FIXTURE_TEST_CASE ( Get_NotFound, Id2name_Fixture )
{
    REQUIRE_RC ( Id2Name_Add( & m_self, 1, "name" ) );
    REQUIRE_RC_FAIL ( Id2Name_Get ( & m_self, 2, & m_res ) );
}
FIXTURE_TEST_CASE ( AddMultiple_Get, Id2name_Fixture )
{
    REQUIRE_RC ( Id2Name_Add( & m_self, 1, "name1" ) );
    REQUIRE_RC ( Id2Name_Add( & m_self, 2, "name2" ) );
    REQUIRE_RC ( Id2Name_Add( & m_self, 3, "name3" ) );
    REQUIRE_RC ( Id2Name_Add( & m_self, 4, "name4" ) );
    REQUIRE_RC ( Id2Name_Add( & m_self, 5, "name5" ) );
    REQUIRE_RC ( Id2Name_Add( & m_self, 6, "name6" ) );

    REQUIRE_RC ( Id2Name_Get ( & m_self, 1, & m_res ) );
    REQUIRE_EQ ( string ( "name1" ), string ( m_res ) );

    REQUIRE_RC ( Id2Name_Get ( & m_self, 4, & m_res ) );
    REQUIRE_EQ ( string ( "name4" ), string ( m_res ) );

    REQUIRE_RC ( Id2Name_Get ( & m_self, 6, & m_res ) );
    REQUIRE_EQ ( string ( "name6" ), string ( m_res ) );

    REQUIRE_RC_FAIL ( Id2Name_Get ( & m_self, 0, & m_res ) );
    REQUIRE_RC_FAIL ( Id2Name_Get ( & m_self, 7, & m_res ) );
    REQUIRE_RC_FAIL ( Id2Name_Get ( & m_self, 123456, & m_res ) );
}

FIXTURE_TEST_CASE ( AddRealloc, Id2name_Fixture )
{   // exceed the original chunk of 1024*1024 bytes
    string str ( 1024 * 1024 + 1, '.');
    REQUIRE_RC ( Id2Name_Add( & m_self, 1, str.c_str() ) );
    REQUIRE_RC ( Id2Name_Get ( & m_self, 1, & m_res ) );
    REQUIRE_EQ ( str, string ( m_res ) );
}

FIXTURE_TEST_CASE ( MiltipleBuffers, Id2name_Fixture )
{   // exceed a single KDataBuffer's size limit of 2**32-1 bytes
    string str1 ( 0x7fffffff, '1');
    REQUIRE_RC ( Id2Name_Add( & m_self, 1, str1.c_str() ) );
    string str2 ( 0x7fffffff, '2');
    REQUIRE_RC ( Id2Name_Add( & m_self, 2, str2.c_str() ) );
    REQUIRE_RC ( Id2Name_Get ( & m_self, 1, & m_res ) );
    REQUIRE_EQ ( str1, string ( m_res ) );
    REQUIRE_RC ( Id2Name_Get ( & m_self, 2, & m_res ) );
    REQUIRE_EQ ( str2, string ( m_res ) );
}

FIXTURE_TEST_CASE ( NameTooLong, Id2name_Fixture )
{   // reject names exceeding KDataBuffer's size limit of 2**32-1 bytes
    string str;
    try
    {
        str = string ( 0x800000000, '1');
    }
    catch(const std::bad_alloc& e)
    {   // this machine does not have enough memory
        return;
    }

    REQUIRE_RC_FAIL ( Id2Name_Add( & m_self, 1, str.c_str() ) );
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return Id2NameSuite(argc, argv);
}

}
