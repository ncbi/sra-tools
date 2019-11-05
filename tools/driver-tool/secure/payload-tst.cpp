/*==============================================================================
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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#include <gtest/gtest.h>

#define SEC_TESTING 1
#define PAYLOAD_TESTING 1

#include "payload.cpp"

#include <string>
#include <cstring>

namespace ncbi
{

    class PayloadTestFixture : public :: testing :: Test
    {
    public:

        virtual void SetUp () override
        {
        }

        virtual void TearDown () override
        {
        }

    protected:

        Payload pay;

    };

    TEST ( PayloadTest, constructor_destructor )
    {
        Payload pay;
    }

    TEST ( PayloadTest, copy_constructor_op )
    {
        Payload pay;
        Payload p2 ( pay );
        pay = p2;
    }

    TEST ( PayloadTest, move_constructor_op )
    {
        Payload pay;
        Payload p2 ( std :: move ( pay ) );
        pay = std :: move ( p2 );
    }

    TEST ( PayloadTest, construct_with_initial_cap )
    {
        Payload pay ( 752 );
    }

    TEST_F ( PayloadTestFixture, access_with_zero_cap )
    {
        EXPECT_EQ ( pay . data (), nullptr );
        EXPECT_EQ ( const_cast < const Payload * > ( & pay ) -> data (), nullptr );
        EXPECT_EQ ( pay . size (), 0U );
        EXPECT_EQ ( pay . capacity (), 0U );
    }

    TEST_F ( PayloadTestFixture, set_capacity )
    {
        EXPECT_ANY_THROW ( pay . setSize ( 12345 ) );
        EXPECT_EQ ( pay . size (), 0U );
        pay . increaseCapacity ( 12345 );
        EXPECT_NE ( pay . data (), nullptr );
        EXPECT_NE ( const_cast < const Payload * > ( & pay ) -> data (), nullptr );
        EXPECT_EQ ( pay . size (), 0U );
        EXPECT_EQ ( pay . capacity (), 12345U );
        EXPECT_ANY_THROW ( pay . setSize ( 12346 ) );
        pay . setSize ( 12345 );
        EXPECT_EQ ( pay . size (), 12345U );
        EXPECT_EQ ( pay . capacity (), 12345U );
    }

    TEST_F ( PayloadTestFixture, fill_wipe_and_reinit )
    {
        pay . increaseCapacity ();
        EXPECT_EQ ( pay . capacity (), 256U );
        const char msg [] = "secret key data";
        memmove ( pay . data (), msg, sizeof msg );
        pay . setSize ( sizeof msg );
        EXPECT_STREQ ( ( const char * ) pay . data (), msg );
        pay . wipe ();
        EXPECT_STREQ ( ( const char * ) pay . data (), "" );
        EXPECT_EQ ( pay . size (), sizeof msg );
        EXPECT_EQ ( pay . capacity (), 256U );
        pay . reinitialize ( false );
        EXPECT_EQ ( pay . size (), 0U );
        EXPECT_EQ ( pay . capacity (), 0U );
    }

    TEST_F ( PayloadTestFixture, fill_and_increase )
    {
        pay . increaseCapacity ();
        EXPECT_EQ ( pay . capacity (), 256U );
        memset ( pay . data (), 'A', pay . capacity () );
        pay . setSize ( pay . capacity () );

        pay . increaseCapacity ( 1234567 );
        EXPECT_EQ ( pay . capacity (), 256U + 1234567U );
        memset ( ( char * ) pay . data () + 256, 'B', pay . capacity () - 256 );
        pay . setSize ( pay . capacity () );

        size_t i;
        const char * cp = ( const char * ) pay . data ();
        for ( i = 0; i < 256; ++ i )
        {
            EXPECT_EQ ( cp [ i ], 'A' );
        }
        for ( ; i < pay . size (); ++ i )
        {
            EXPECT_EQ ( cp [ i ], 'B' );
        }
    }

}

extern "C"
{
    int main ( int argc, const char * argv [], const char * envp []  )
    {
        testing :: InitGoogleTest ( & argc, ( char ** ) argv );
        return RUN_ALL_TESTS ();
    }
}
