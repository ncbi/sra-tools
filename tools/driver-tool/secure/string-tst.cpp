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
#define STRING_TESTING 1

#include "string.cpp"

#include <string>
#include <cstring>

namespace ncbi
{

    /*=================================================*
     *                   StringTest                    *
     *=================================================*/

    TEST ( StringTest, constructor_destructor )
    {
        String str;
    }

    TEST ( StringTest, constructor_from_manifest_cstring_constant )
    {
        String str1 ( "a manifest string" );
        String str2 = "another manifest string";
    }

    TEST ( StringTest, constructor_from_manifest_string_constant_and_size )
    {
        const UTF8 utf8 [] = "a manifest string";
        String str ( utf8, sizeof utf8 - 1 );
        EXPECT_EQ ( str . size (), sizeof utf8 - 1 );
        EXPECT_EQ ( str . count (), sizeof utf8 - 1 );
        EXPECT_EQ ( str . length (), sizeof utf8 - 1 );
    }

    TEST ( StringTest, constructor_from_manifest_unicode_cstring_constant )
    {
        const UTF8 utf8 [] = "Einige Bücher";
        String str = utf8;
        EXPECT_EQ ( str . size (), sizeof utf8 - 1 );
        EXPECT_EQ ( str . count (), sizeof utf8 - 2 );
        EXPECT_EQ ( str . length (), sizeof utf8 - 2 );
    }

    TEST ( StringTest, constructor_from_stl_ascii_string )
    {
        std :: string ascii ( "a manifest string" );
        String str ( ascii );
        EXPECT_EQ ( str . size (), ascii . size () );
        EXPECT_EQ ( str . count (), ascii . size () );
        EXPECT_EQ ( str . length (), ascii . size () );
    }

    TEST ( StringTest, constructor_from_stl_unicode_string )
    {
        std :: string utf8 ( "Einige Bücher" );
        String str ( utf8 );
        EXPECT_EQ ( str . size (), utf8 . size () );
        EXPECT_EQ ( str . count (), utf8 . size () - 1 );
        EXPECT_EQ ( str . length (), utf8 . size () - 1 );
    }

    TEST ( StringTest, constructor_from_stl_ASCII_string_with_control_char )
    {
        // NB - the "\x07" escape sequence is allowed to extend
        // to as many hex digits as it wants (great), and collides
        // with the 'd' of "doo" and essentially creates a different
        // character.
        std :: string utf8 ( "zippity\t\x07" "doo\ndah!" );
        String str ( utf8 );
        EXPECT_EQ ( str . size (), utf8 . size () );
        EXPECT_EQ ( str . count (), utf8 . size () );
        EXPECT_EQ ( str . length (), utf8 . size () );
        EXPECT_EQ ( str [ 7 ], ( UTF32 ) '\t' );
        EXPECT_EQ ( str [ 8 ], ( UTF32 ) 7 );
        EXPECT_EQ ( str [ 9 ], ( UTF32 ) 'd' );
    }

    TEST ( StringTest, constructor_from_ascii_utf16 )
    {
        const UTF8 utf8 [] = "a manifest string";
        UTF16 utf16 [ sizeof utf8 ];
        for ( size_t i = 0; i < sizeof utf8; ++ i )
            utf16 [ i ] = utf8 [ i ];
        String str ( utf16 );
        EXPECT_EQ ( str . size (), sizeof utf8 - 1 );
        EXPECT_EQ ( str . count (), sizeof utf8 - 1 );
        EXPECT_EQ ( str . length (), sizeof utf8 - 1 );
    }

    TEST ( StringTest, constructor_from_unicode_utf16 )
    {
        const char utf8 [] = "Einige Bücher";

        size_t i, offset;
        UTF32 utf32 [ sizeof utf8 ];
        for ( i = offset = 0; offset < sizeof utf8 - 1; ++ i )
        {
            utf32 [ i ] = utf8_to_utf32 ( utf8, sizeof utf8 - 1, offset );
            offset += scan_fwd ( utf8, sizeof utf8 - 1, offset );
        }
        utf32 [ i ] = 0;
        
        UTF16 utf16 [ sizeof utf8 ];
        for ( i = offset = 0; utf32 [ i ] != 0; ++ offset, ++ i )
        {
            if ( utf32 [ i ] < 0x10000 )
                utf16 [ offset ] = utf32 [ i ];
            else
            {
                UTF32 ch = utf32 [ i ] - 0x10000;
                utf16 [ offset + 0 ] = ( ch >> 10 ) + 0xD800;
                utf16 [ offset + 1 ] = ( ch & 0x3FF ) + 0xDC00;
                ++ offset;
            }
        }
        utf16 [ offset ] = 0;

        String str ( utf16 );
        EXPECT_EQ ( str . size (), sizeof utf8 - 1 );
        EXPECT_EQ ( str . count (), sizeof utf8 - 2 );
        EXPECT_EQ ( str . length (), sizeof utf8 - 2 );
    }

    TEST ( StringTest, constructor_from_unicode_utf32 )
    {
        const char utf8 [] = "Einige Bücher";

        size_t i, offset;
        UTF32 utf32 [ sizeof utf8 ];
        for ( i = offset = 0; offset < sizeof utf8 - 1; ++ i )
        {
            utf32 [ i ] = utf8_to_utf32 ( utf8, sizeof utf8 - 1, offset );
            offset += scan_fwd ( utf8, sizeof utf8 - 1, offset );
        }
        utf32 [ i ] = 0;

        String str ( utf32 );
        EXPECT_EQ ( str . size (), sizeof utf8 - 1 );
        EXPECT_EQ ( str . count (), sizeof utf8 - 2 );
        EXPECT_EQ ( str . length (), sizeof utf8 - 2 );
    }

    TEST ( StringTest, copy_constructor_op )
    {
        String str1 = "abc";
        String str2 ( str1 );
        str1 = str2;
    }

    TEST ( StringTest, predicates )
    {
        String str1;
        EXPECT_EQ ( str1 . isEmpty (), true );
        EXPECT_EQ ( str1 . isAscii (), true );

        String str2 = "abc";
        EXPECT_EQ ( str2 . isEmpty (), false );
        EXPECT_EQ ( str2 . isAscii (), true );

        String str3 = "¿dónde están las uvas?";
        EXPECT_EQ ( str3 . isEmpty (), false );
        EXPECT_EQ ( str3 . isAscii (), false );
    }

    TEST ( StringTest, properties )
    {
        String str1;
        EXPECT_EQ ( str1 . size (), 0U );
        EXPECT_EQ ( str1 . count (), 0U );

        std :: string ascii = "abc";
        String str2 ( ascii );
        EXPECT_EQ ( str2 . size (), ascii . size () );
        EXPECT_EQ ( str2 . count (), ascii . size () );

        std :: string utf8 = "¿dónde están las uvas?";
        String str3 ( utf8 );
        EXPECT_EQ ( str3 . size (), utf8 . size () );
        EXPECT_EQ ( str3 . count (), utf8 . size () - 3 );
    }

    TEST ( StringTest, accessors )
    {
        String str1;
        EXPECT_NE ( str1 . data (), nullptr );
        EXPECT_ANY_THROW ( str1 . getChar ( 0 ) );
        EXPECT_ANY_THROW ( str1 . getChar ( 1 ) );

        std :: string ascii = "abc";
        String str2 ( ascii );
        EXPECT_NE ( str2 . data (), nullptr );
        EXPECT_EQ ( str2 . getChar ( 0 ), ( UTF32 ) 'a' );
        EXPECT_EQ ( str2 . getChar ( 1 ), ( UTF32 ) 'b' );
        EXPECT_EQ ( str2 . getChar ( 2 ), ( UTF32 ) 'c' );
        EXPECT_ANY_THROW ( str2 . getChar ( 3 ) );

        std :: string utf8 = "¿dónde están las uvas?";
        String str3 ( utf8 );
        EXPECT_NE ( str3 . data (), nullptr );
        EXPECT_EQ ( str3 . getChar (  0 ), ( UTF32 ) 0xBF );
        EXPECT_EQ ( str3 . getChar (  1 ), ( UTF32 )  'd' );
        EXPECT_EQ ( str3 . getChar (  2 ), ( UTF32 ) 0xF3 );
        EXPECT_EQ ( str3 . getChar (  3 ), ( UTF32 )  'n' );
        EXPECT_EQ ( str3 . getChar (  4 ), ( UTF32 )  'd' );
        EXPECT_EQ ( str3 . getChar (  5 ), ( UTF32 )  'e' );
        EXPECT_EQ ( str3 . getChar (  6 ), ( UTF32 )  ' ' );
        EXPECT_EQ ( str3 . getChar (  7 ), ( UTF32 )  'e' );
        EXPECT_EQ ( str3 . getChar (  8 ), ( UTF32 )  's' );
        EXPECT_EQ ( str3 . getChar (  9 ), ( UTF32 )  't' );
        EXPECT_EQ ( str3 . getChar ( 10 ), ( UTF32 ) 0xE1 );
        EXPECT_EQ ( str3 . getChar ( 11 ), ( UTF32 )  'n' );
        EXPECT_EQ ( str3 . getChar ( 12 ), ( UTF32 )  ' ' );
        EXPECT_EQ ( str3 . getChar ( 13 ), ( UTF32 )  'l' );
        EXPECT_EQ ( str3 . getChar ( 14 ), ( UTF32 )  'a' );
        EXPECT_EQ ( str3 . getChar ( 15 ), ( UTF32 )  's' );
        EXPECT_EQ ( str3 . getChar ( 16 ), ( UTF32 )  ' ' );
        EXPECT_EQ ( str3 . getChar ( 17 ), ( UTF32 )  'u' );
        EXPECT_EQ ( str3 . getChar ( 18 ), ( UTF32 )  'v' );
        EXPECT_EQ ( str3 . getChar ( 19 ), ( UTF32 )  'a' );
        EXPECT_EQ ( str3 . getChar ( 20 ), ( UTF32 )  's' );
        EXPECT_EQ ( str3 . getChar ( 21 ), ( UTF32 )  '?' );
        EXPECT_ANY_THROW ( str3 . getChar ( 22 ) );

        // perform a slight shuffle to ensure lack of stateful behavior
        EXPECT_EQ ( str3 . getChar ( 14 ), ( UTF32 )  'a' );
        EXPECT_EQ ( str3 . getChar (  1 ), ( UTF32 )  'd' );
        EXPECT_EQ ( str3 . getChar (  2 ), ( UTF32 ) 0xF3 );
        EXPECT_EQ ( str3 . getChar (  3 ), ( UTF32 )  'n' );
        EXPECT_EQ ( str3 . getChar (  4 ), ( UTF32 )  'd' );
        EXPECT_EQ ( str3 . getChar (  5 ), ( UTF32 )  'e' );
        EXPECT_EQ ( str3 . getChar ( 17 ), ( UTF32 )  'u' );
        EXPECT_EQ ( str3 . getChar (  6 ), ( UTF32 )  ' ' );
        EXPECT_EQ ( str3 . getChar (  0 ), ( UTF32 ) 0xBF );
        EXPECT_EQ ( str3 . getChar (  7 ), ( UTF32 )  'e' );
        EXPECT_EQ ( str3 . getChar (  8 ), ( UTF32 )  's' );
        EXPECT_EQ ( str3 . getChar (  9 ), ( UTF32 )  't' );
        EXPECT_EQ ( str3 . getChar ( 10 ), ( UTF32 ) 0xE1 );
        EXPECT_EQ ( str3 . getChar ( 11 ), ( UTF32 )  'n' );
        EXPECT_EQ ( str3 . getChar ( 12 ), ( UTF32 )  ' ' );

        // TBD - 3 and 4 byte UTF-8 characters
    }

    TEST ( StringTest, iterator )
    {
        std :: string utf8 = "¿dónde están las uvas?";
        String str ( utf8 );
        String :: Iterator it = str . makeIterator ();

        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 0U );
        EXPECT_EQ ( it . byteOffset (), 0U );
        EXPECT_EQ ( * it, ( UTF32 ) 0xBF );

        ++ it;
        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 1U );
        EXPECT_EQ ( it . byteOffset (), 2U );
        EXPECT_EQ ( * it, ( UTF32 ) 'd' );

        ++ it;
        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 2U );
        EXPECT_EQ ( it . byteOffset (), 3U );
        EXPECT_EQ ( * it, ( UTF32 ) 0xF3 );

        ++ it;
        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 3 );
        EXPECT_EQ ( it . byteOffset (), 5 );
        EXPECT_EQ ( * it, ( UTF32 ) 'n' );

        ++ it;
        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 4 );
        EXPECT_EQ ( it . byteOffset (), 6 );
        EXPECT_EQ ( * it, ( UTF32 ) 'd' );

        ++ it; //  5
        ++ it; //  6
        ++ it; //  7
        ++ it; //  8
        ++ it; //  9

        ++ it;
        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 10 );
        EXPECT_EQ ( it . byteOffset (), 12 );
        EXPECT_EQ ( * it, ( UTF32 ) 0xE1 );

        ++ it;
        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 11 );
        EXPECT_EQ ( it . byteOffset (), 14 );
        EXPECT_EQ ( * it, ( UTF32 ) 'n' );

        ++ it; // 12
        ++ it; // 13
        ++ it; // 14
        ++ it; // 15
        ++ it; // 16
        ++ it; // 17
        ++ it; // 18
        ++ it; // 19
        ++ it; // 20

        ++ it;
        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 21 );
        EXPECT_EQ ( it . byteOffset (), 24 );
        EXPECT_EQ ( * it, ( UTF32 ) '?' );

        ++ it;
        EXPECT_EQ ( it . isValid (), false );
        EXPECT_EQ ( it . charIndex (), 22 );
        EXPECT_EQ ( it . byteOffset (), 25 );
        EXPECT_ANY_THROW ( * it );

        -- it; // 21
        -- it; // 20
        -- it; // 19
        -- it; // 18
        -- it; // 17
        -- it; // 16

        -- it;
        EXPECT_EQ ( it . isValid (), true );
        EXPECT_EQ ( it . charIndex (), 15 );
        EXPECT_EQ ( it . byteOffset (), 18 );
        EXPECT_EQ ( * it, ( UTF32 ) 's' );
    }

    /*=================================================*
     *             NULTerminatedStringTest             *
     *=================================================*/

    TEST ( NULTermStringTest, test_c_str )
    {
        std :: string utf8 = "¿dónde están las uvas?";
        String str ( utf8 );
        NULTerminatedString zstr ( str );
        EXPECT_STREQ ( utf8 . c_str (), zstr . c_str () );
    }

    /*=================================================*
     *                StringBufferTest                 *
     *=================================================*/

    TEST ( StringBufferTest, constructor_destructor )
    {
        StringBuffer sb;
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
