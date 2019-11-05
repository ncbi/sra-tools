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
#define EXCEPT_TESTING 1

#include "except.cpp"

#include <string>
#include <cstring>

namespace ncbi
{

    TEST ( XPTest, constructor_destructor )
    {
        XP xp ( XLOC );
    }

    TEST ( XPTest, bool_op )
    {
        bool val = true;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, short_int_op )
    {
        short int val = -1234;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, unsigned_short_int_op )
    {
        unsigned short int val = 1234;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, int_op )
    {
        int val = -123456;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, unsigned_int_op )
    {
        unsigned int val = 123456;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, long_int_op )
    {
        long int val = -12345678L;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, unsigned_long_int_op )
    {
        unsigned long int val = 12345678UL;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, long_long_int_op )
    {
        long long int val = -1234567890987654321LL;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, unsigned_long_long_int_op )
    {
        unsigned long long int val = 1234567890987654321ULL;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, float_op )
    {
        float val = 1.23456;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, double_op )
    {
        double val = 1.234567890987;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, long_double_op )
    {
        long double val = 1.23456789098765432123456789;
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, func_op )
    {
        XP xp ( XLOC );
        xp << xprob << xctx << xcause << xsuggest;
    }

    TEST ( XPTest, std_string_op )
    {
        std :: string val ( "bing bang boom" );
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, XMsg_op )
    {
        const ASCII str [] = "bing bang boom";
        XMsg val;
        val . msg_size = sizeof str - 1;
        memmove ( val . zmsg, str, sizeof str );
        XP xp ( XLOC );
        xp << val;
    }

    TEST ( XPTest, putChar_ASCII )
    {
        UTF32 ch = 'A';
        XP xp ( XLOC );
        xp . putChar ( ch );
    }

    TEST ( XPTest, putChar_UTF32 )
    {
        UTF32 ch = 252; // 'ü'
        XP xp ( XLOC );
        xp . putChar ( ch );
    }

    TEST ( XPTest, putUTF8_zascii )
    {
        const UTF8 * str = "really ascii";
        XP xp ( XLOC );
        xp . putUTF8 ( str );
    }

    TEST ( XPTest, putUTF8_ascii_sz )
    {
        const UTF8 str [] = "really ascii";
        XP xp ( XLOC );
        xp . putUTF8 ( str, sizeof str );
    }

    TEST ( XPTest, putUTF8_zutf8 )
    {
        const UTF8 * str = "Einige Bücher";
        XP xp ( XLOC );
        xp . putUTF8 ( str );
    }

    TEST ( XPTest, putUTF8_utf8_sz )
    {
        const UTF8 * str = "Einige Bücher";
        XP xp ( XLOC );
        xp . putUTF8 ( str, sizeof str );
    }

    TEST ( XPTest, putPtr )
    {
        const UTF8 * str = "Einige Bücher";
        XP xp ( XLOC );
        xp . putPtr ( str );
    }

    TEST ( XPTest, setRadix )
    {
        XP xp ( XLOC );
        for ( unsigned int r = 0; r < 64; ++ r )
        {
            xp . setRadix ( r );
            xp << 1234;
        }
    }

    TEST ( XPTest, sysError )
    {
        XP xp ( XLOC );
        xp . sysError ( 1 );         // EPERM
    }

    TEST ( XPTest, cryptError )
    {
        XP xp ( XLOC );
        xp . cryptError ( -0x0062 ); // MBEDTLS_ERR_ASN1_UNEXPECTED_TAG
    }

    TEST ( XPTest, global_char_op )
    {
        char val = 'n';
        XP xp ( XLOC );
        xp << val << '\n';
    }

    TEST ( XPTest, global_UTF32_char_op )
    {
        UTF32 val = 252;
        XP xp ( XLOC );
        xp << putc ( val ) << '\n';
    }

    TEST ( XPTest, global_UTF8_zstr_op )
    {
        XP xp ( XLOC );
        xp << "Einige Bücher" << '\n';
    }

    TEST ( XPTest, putUTF8_xwhat_xcause_xsuggest )
    {
        XP xp ( XLOC );
        xp
            << xprob
            << "this goes to the problem msg"
            << xctx
            << "this goes to the context msg"
            << xcause
            << "and this goes to the cause msg"
            << xsuggest
            << "and finally this goes to the suggestion"
            ;
    }

    TEST ( XPTest, global_radix_ops )
    {
        XP xp ( XLOC );
        xp
            << binary
            << 1234
            << octal
            << 1234
            << decimal
            << 1234
            << hex
            << 1234
            << radix ( 36 )
            << 1234
            << '\n'
            ;
    }

    TEST ( XPTest, global_error_ops )
    {
        XP xp ( XLOC );
        xp
            << syserr ( 1 )
            << crypterr ( -0x0062 )
            << '\n'
            ;
    }

    TEST ( XPTest, global_format_op )
    {
        XP xp ( XLOC );
        xp
            << format ( "whacky format %d\n", 1234 )
            ;
    }

    TEST ( ExceptionTest, constructor_destructor )
    {
        Exception ( XP ( XLOC ) );
    }

    TEST ( ExceptionTest, copy_op_constructor )
    {
        Exception x ( XP ( XLOC ) );
        Exception x1 ( x );
        x = x1;
    }

    TEST ( ExceptionTest, file_path )
    {
        Exception x ( XP ( XLOC ) );
        EXPECT_STREQ ( x . file () . zmsg, "secure/except-tst.cpp" );
    }

    TEST ( ExceptionTest, lineno )
    {
        Exception x ( XP ( XLOC ) );
        EXPECT_EQ ( x . line (), ( unsigned int ) __LINE__ - 1U );
    }

    TEST ( ExceptionTest, function )
    {
        Exception x ( XP ( XLOC ) );
        EXPECT_STREQ ( x . function () . zmsg, __func__ );
    }

    TEST ( ExceptionTest, simple_what )
    {
        const char str [] = "simple what message";
        Exception x (
            XP ( XLOC )
            << str
            );
        const XMsg what = x . what ();
        EXPECT_EQ ( what . msg_size, sizeof str - 1 );
        EXPECT_STREQ ( what . zmsg, str );
    }

    TEST ( ExceptionTest, combined_what_cause_suggest )
    {
        const char pstr [] = "simple problem message";
        const char xstr [] = "simple context message";
        const char cstr [] = "simple cause message";
        const char sstr [] = "simple suggestion";

        Exception x (
            XP ( XLOC )
            << xprob
            << pstr
            << xctx
            << xstr
            << xcause
            << cstr
            << xsuggest
            << sstr
            );

        const XMsg prob = x . problem ();
        EXPECT_EQ ( prob . msg_size, sizeof pstr - 1 );
        EXPECT_STREQ ( prob . zmsg, pstr );

        const XMsg ctx = x . context ();
        EXPECT_EQ ( ctx . msg_size, sizeof xstr - 1 );
        EXPECT_STREQ ( ctx . zmsg, xstr );

        const XMsg cause = x . cause ();
        EXPECT_EQ ( cause . msg_size, sizeof cstr - 1 );
        EXPECT_STREQ ( cause . zmsg, cstr );

        const XMsg sug = x . suggestion ();
        EXPECT_EQ ( sug . msg_size, sizeof sstr - 1 );
        EXPECT_STREQ ( sug . zmsg, sstr );
    }

    TEST ( ExceptionTest, combined_standard_construct )
    {
        std :: string s ( "standard" );
        Exception x (
            XP ( XLOC )
            << "a more '"
            << s
            << "' scenario, size "
            << s . size ()
            << " ( 0x"
            << hex
            << s . size ()
            << " ) [ 0b"
            << binary
            << s . size ()
            << " ]"
            );

        const XMsg what = x . what ();
        EXPECT_EQ ( what . msg_size, 53U );
        EXPECT_STREQ ( what . zmsg, "a more 'standard' scenario, size 8 ( 0x8 ) [ 0b1000 ]" );
    }

    TEST ( ExceptionTest, combined_UTF8_construct )
    {
        UTF32 ch = 252; // 'ü'
        std :: string s ( "Einige Bücher" );
        Exception x (
            XP ( XLOC )
            << "and now the '"
            << putc ( ch )
            << "' ( 0b"
            << binary
            << ch
            << decimal
            << " ) as seen in the string '"
            << s
            << "' of size "
            << s . size ()
            << " with character count 13"
            );

        const XMsg what = x . what ();
        EXPECT_EQ ( what . msg_size, 105U );
        EXPECT_STREQ ( what . zmsg, "and now the 'ü' ( 0b11111100 ) as seen in the string 'Einige Bücher' of size 14 with character count 13" );
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
