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
 * ===========================================================================
 *
 */

#include <ncbi/test/gtest.hpp>

#undef JSON_TESTING
#define JSON_TESTING 1

#include "json-primitive.cpp"
#include "json-wrapper.cpp"
#include "json-value.cpp"
#include "json-array.cpp"
#include "json-object.cpp"
#include "json.cpp"

#include <string>
#include <cstring>

namespace ncbi
{

    /*=================================================*
     *                 JSONPrimitive                   *
     *=================================================*/

    TEST ( JSONBooleanTest, constructor_destructor )
    {
        JSONBoolean prim ( true );
    }

    TEST ( JSONBooleanTest, accessors )
    {
        JSONBoolean prim ( true );
        EXPECT_EQ ( prim . toBoolean (), true );
        EXPECT_EQ ( prim . toString (), String ( "true" ) );
        EXPECT_EQ ( prim . toJSON (), String ( "true" ) );

        JSONPrimitive * clone = prim . clone ();
        EXPECT_NE ( clone, nullptr );
        EXPECT_EQ ( ( ( const JSONBoolean * ) clone ) -> toBoolean (), prim . toBoolean () );

        clone -> invalidate ();
        EXPECT_EQ ( ( ( const JSONBoolean * ) clone ) -> toBoolean (), false );
        delete clone;
    }

    TEST ( JSONIntegerTest, constructor_destructor )
    {
        JSONInteger prim ( -1234567 );
    }

    TEST ( JSONIntegerTest, accessors )
    {
        JSONInteger prim ( -1234567 );
        EXPECT_EQ ( prim . toInteger (), -1234567 );
        EXPECT_EQ ( prim . toString (), String ( "-1234567" ) );
        EXPECT_EQ ( prim . toJSON (), String ( "-1234567" ) );

        JSONPrimitive * clone = prim . clone ();
        EXPECT_NE ( clone, nullptr );
        EXPECT_EQ ( ( ( const JSONInteger * ) clone ) -> toInteger (), prim . toInteger () );

        clone -> invalidate ();
        EXPECT_EQ ( ( ( const JSONInteger * ) clone ) -> toInteger (), 0 );
        delete clone;
    }

    TEST ( JSONNumberTest, constructor_destructor )
    {
        JSONNumber prim ( "-1234567.89" );
    }

    TEST ( JSONNumberTest, accessors )
    {
        JSONNumber prim ( "-1234567.89" );
        EXPECT_EQ ( prim . toString (), String ( "-1234567.89" ) );
        EXPECT_EQ ( prim . toJSON (), String ( "-1234567.89" ) );

        JSONPrimitive * clone = prim . clone ();
        EXPECT_NE ( clone, nullptr );
        EXPECT_EQ ( clone -> toString (), prim . toString () );

        clone -> invalidate ();
        EXPECT_EQ ( clone -> toString (), String ( "" ) );
        delete clone;
    }

    TEST ( JSONStringTest, constructor_destructor )
    {
        JSONString prim ( "wonderful" );
    }

    TEST ( JSONStringTest, accessors )
    {
        JSONString prim ( "wonderful!\n\"un día más del año\"\tsaid the dog" );
        EXPECT_EQ ( prim . toString (), String ( "wonderful!\n\"un día más del año\"\tsaid the dog" ) );
        EXPECT_EQ ( prim . toJSON (), String ( "\"wonderful!\\n\\\"un día más del año\\\"\\tsaid the dog\"" ) );

        JSONPrimitive * clone = prim . clone ();
        EXPECT_NE ( clone, nullptr );
        EXPECT_EQ ( clone -> toString (), prim . toString () );

        clone -> invalidate ();
        EXPECT_EQ ( clone -> toString (), String ( "" ) );
        delete clone;
    }

    /*=================================================*
     *                   JSONValue                     *
     *=================================================*/

    TEST ( JSONWrapper, constructor_destructor )
    {
        JSONWrapper null_val ( jvt_null );
        JSONWrapper bool_val ( jvt_bool, new JSONBoolean ( false ) );
        JSONWrapper int_val ( jvt_int, new JSONInteger ( -1234567 ) );
        JSONWrapper num_val ( jvt_num, new JSONNumber ( "-1234567.89" ) );
        JSONWrapper str_val ( jvt_str, new JSONString ( "whippity doo" ) );
    }

    TEST ( JSONWrapper, assignment )
    {
        JSONValueRef ref = new JSONWrapper ( jvt_null );
        JSONValue & val = * ref;

        EXPECT_EQ ( val . isNull (), true );
        EXPECT_EQ ( val . isBoolean (), false );
        EXPECT_EQ ( val . isNumber (), false );
        EXPECT_EQ ( val . isInteger (), false );
        EXPECT_EQ ( val . isString (), false );
        EXPECT_EQ ( val . isArray (), false );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_ANY_THROW ( val . toBoolean () );
        EXPECT_ANY_THROW ( val . toNumber () );
        EXPECT_ANY_THROW ( val . toInteger () );
        EXPECT_EQ ( val . toString (), String ( "null" ) );
        EXPECT_EQ ( val . toJSON (), String ( "null" ) );
        EXPECT_ANY_THROW ( val . toArray () );
        EXPECT_ANY_THROW ( val . toObject () );

        val . setBoolean ( true );
        EXPECT_EQ ( val . isNull (), false );
        EXPECT_EQ ( val . isBoolean (), true );
        EXPECT_EQ ( val . isNumber (), false );
        EXPECT_EQ ( val . isInteger (), false );
        EXPECT_EQ ( val . isString (), false );
        EXPECT_EQ ( val . isArray (), false );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_EQ ( val . toBoolean (), true );
        EXPECT_ANY_THROW ( val . toNumber () );
        EXPECT_ANY_THROW ( val . toInteger () );
        EXPECT_EQ ( val . toString (), String ( "true" ) );
        EXPECT_EQ ( val . toJSON (), String ( "true" ) );
        EXPECT_ANY_THROW ( val . toArray () );
        EXPECT_ANY_THROW ( val . toObject () );

        val . setBoolean ( false );
        EXPECT_EQ ( val . isNull (), false );
        EXPECT_EQ ( val . isBoolean (), true );
        EXPECT_EQ ( val . isNumber (), false );
        EXPECT_EQ ( val . isInteger (), false );
        EXPECT_EQ ( val . isString (), false );
        EXPECT_EQ ( val . isArray (), false );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_EQ ( val . toBoolean (), false );
        EXPECT_ANY_THROW ( val . toNumber () );
        EXPECT_ANY_THROW ( val . toInteger () );
        EXPECT_EQ ( val . toString (), String ( "false" ) );
        EXPECT_EQ ( val . toJSON (), String ( "false" ) );
        EXPECT_ANY_THROW ( val . toArray () );
        EXPECT_ANY_THROW ( val . toObject () );

        val . setInteger ( 1024 );
        EXPECT_EQ ( val . isNull (), false );
        EXPECT_EQ ( val . isBoolean (), false );
        EXPECT_EQ ( val . isNumber (), true );
        EXPECT_EQ ( val . isInteger (), true );
        EXPECT_EQ ( val . isString (), false );
        EXPECT_EQ ( val . isArray (), false );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_ANY_THROW ( val . toBoolean () );
        EXPECT_EQ ( val . toNumber (), String ( "1024" ) );
        EXPECT_EQ ( val . toInteger (), 1024 );
        EXPECT_EQ ( val . toString (), String ( "1024" ) );
        EXPECT_EQ ( val . toJSON (), String ( "1024" ) );
        EXPECT_ANY_THROW ( val . toArray () );
        EXPECT_ANY_THROW ( val . toObject () );

        val . setNumber ( "1024" );
        EXPECT_EQ ( val . isNull (), false );
        EXPECT_EQ ( val . isBoolean (), false );
        EXPECT_EQ ( val . isNumber (), true );
        EXPECT_EQ ( val . isInteger (), false );
        EXPECT_EQ ( val . isString (), false );
        EXPECT_EQ ( val . isArray (), false );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_ANY_THROW ( val . toBoolean () );
        EXPECT_EQ ( val . toNumber (), String ( "1024" ) );
        EXPECT_ANY_THROW ( val . toInteger () );
        EXPECT_EQ ( val . toString (), String ( "1024" ) );
        EXPECT_EQ ( val . toJSON (), String ( "1024" ) );
        EXPECT_ANY_THROW ( val . toArray () );
        EXPECT_ANY_THROW ( val . toObject () );

        val . setNumber ( "987.654321" );
        EXPECT_EQ ( val . isNull (), false );
        EXPECT_EQ ( val . isBoolean (), false );
        EXPECT_EQ ( val . isNumber (), true );
        EXPECT_EQ ( val . isInteger (), false );
        EXPECT_EQ ( val . isString (), false );
        EXPECT_EQ ( val . isArray (), false );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_ANY_THROW ( val . toBoolean () );
        EXPECT_EQ ( val . toNumber (), String ( "987.654321" ) );
        EXPECT_ANY_THROW ( val . toInteger () );
        EXPECT_EQ ( val . toString (), String ( "987.654321" ) );
        EXPECT_EQ ( val . toJSON (), String ( "987.654321" ) );
        EXPECT_ANY_THROW ( val . toArray () );
        EXPECT_ANY_THROW ( val . toObject () );

        val . setDouble ( 987.654321, 10 );
        EXPECT_EQ ( val . isNull (), false );
        EXPECT_EQ ( val . isBoolean (), false );
        EXPECT_EQ ( val . isNumber (), true );
        EXPECT_EQ ( val . isInteger (), false );
        EXPECT_EQ ( val . isString (), false );
        EXPECT_EQ ( val . isArray (), false );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_ANY_THROW ( val . toBoolean () );
        EXPECT_EQ ( val . toNumber (), String ( "987.654321" ) );
        EXPECT_ANY_THROW ( val . toInteger () );
        EXPECT_EQ ( val . toString (), String ( "987.654321" ) );
        EXPECT_EQ ( val . toJSON (), String ( "987.654321" ) );
        EXPECT_ANY_THROW ( val . toArray () );
        EXPECT_ANY_THROW ( val . toObject () );

        // NB - split the string after the '\x07'
        // or C++ will bring in the 'd' as part of
        // the "character". Could also use octal...
        // or UTF-16 or UTF-32...
        val . setString ( "zippity\t\x07" "doo\ndah!" );
        EXPECT_EQ ( val . isNull (), false );
        EXPECT_EQ ( val . isBoolean (), false );
        EXPECT_EQ ( val . isNumber (), false );
        EXPECT_EQ ( val . isInteger (), false );
        EXPECT_EQ ( val . isString (), true );
        EXPECT_EQ ( val . isArray (), false );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_ANY_THROW ( val . toBoolean () );
        EXPECT_ANY_THROW ( val . toNumber () );
        EXPECT_ANY_THROW ( val . toInteger () );
        EXPECT_EQ ( val . toString (), String ( "zippity\t\007doo\ndah!" ) );
        EXPECT_EQ ( val . toJSON (), String ( "\"zippity\\t\\u0007doo\\ndah!\"" ) );
        EXPECT_ANY_THROW ( val . toArray () );
        EXPECT_ANY_THROW ( val . toObject () );
    }

    /*=================================================*
     *                   JSONArray                     *
     *=================================================*/

    TEST ( JSONArray, constructor_destructor )
    {
        JSONArrayRef ref = JSON :: makeArray ();
    }

    TEST ( JSONArray, accessors )
    {
        JSONArrayRef ref = JSON :: makeArray ();
        const JSONArray & val = * ref;

        EXPECT_EQ ( val . isNull (), false );
        EXPECT_EQ ( val . isBoolean (), false );
        EXPECT_EQ ( val . isNumber (), false );
        EXPECT_EQ ( val . isInteger (), false );
        EXPECT_EQ ( val . isString (), false );
        EXPECT_EQ ( val . isArray (), true );
        EXPECT_EQ ( val . isObject (), false );
        EXPECT_ANY_THROW ( val . toBoolean () );
        EXPECT_ANY_THROW ( val . toNumber () );
        EXPECT_ANY_THROW ( val . toInteger () );
        EXPECT_ANY_THROW ( val . toString () );
        EXPECT_EQ ( val . toJSON (), String ( "[]" ) );
        EXPECT_EQ ( & val . toArray (), & val );
        EXPECT_ANY_THROW ( val . toObject () );

        EXPECT_EQ ( val . isEmpty (), true );
        EXPECT_EQ ( val . count (), 0U );
        EXPECT_EQ ( val . exists ( 10 ), false );
        EXPECT_ANY_THROW ( val . getValue ( 0 ) );

    }

    TEST ( JSONArray, update1 )
    {
        JSONArrayRef ref = JSON :: makeArray ();
        JSONArray & val = * ref;
        const JSONArray & cval = val;

        val . appendValue ( JSON :: makeInteger ( 123 ) );
        EXPECT_EQ ( cval . isEmpty (), false );
        EXPECT_EQ ( cval . count (), 1U );
        EXPECT_EQ ( cval . exists ( 0 ), true );
        EXPECT_EQ ( cval . exists ( 1 ), false );
        EXPECT_EQ ( cval . getValue ( 0 ) . toInteger (), 123 );
        EXPECT_ANY_THROW ( cval . getValue ( 1 ) );
    }

    TEST ( JSONArray, update2 )
    {
        JSONArrayRef ref = JSON :: makeArray ();
        JSONArray & val = * ref;
        const JSONArray & cval = val;

        val . appendValue ( JSON :: makeInteger ( 123 ) );
        EXPECT_EQ ( cval . isEmpty (), false );
        EXPECT_EQ ( cval . count (), 1U );
        EXPECT_EQ ( cval . exists ( 0 ), true );
        EXPECT_EQ ( cval . exists ( 1 ), false );
        EXPECT_EQ ( cval . getValue ( 0 ) . toInteger (), 123 );
        EXPECT_ANY_THROW ( cval . getValue ( 1 ) );
        
        val . appendValue ( JSON :: makeString ( "abc" ) );
        EXPECT_EQ ( cval . isEmpty (), false );
        EXPECT_EQ ( cval . count (), 2U );
        EXPECT_EQ ( cval . exists ( 0 ), true );
        EXPECT_EQ ( cval . exists ( 1 ), true );
        EXPECT_EQ ( cval . exists ( 2 ), false );
        EXPECT_EQ ( cval . getValue ( 0 ) . toInteger (), 123 );
        EXPECT_EQ ( cval . getValue ( 1 ) . toString (), String ( "abc" ) );
        EXPECT_ANY_THROW ( cval . getValue ( 2 ) );
        EXPECT_EQ ( cval . toJSON (), String ( "[123,\"abc\"]" ) );
    }

    TEST ( JSONArray, update3 )
    {
        JSONArrayRef ref = JSON :: makeArray ();
        JSONArray & val = * ref;
        const JSONArray & cval = val;

        val . appendValue ( JSON :: makeInteger ( 123 ) );
        EXPECT_EQ ( cval . isEmpty (), false );
        EXPECT_EQ ( cval . count (), 1U );
        EXPECT_EQ ( cval . exists ( 0 ), true );
        EXPECT_EQ ( cval . exists ( 1 ), false );
        EXPECT_EQ ( cval . getValue ( 0 ) . toInteger (), 123 );
        EXPECT_ANY_THROW ( cval . getValue ( 1 ) );
        
        val . appendValue ( JSON :: makeString ( "abc" ) );
        EXPECT_EQ ( cval . isEmpty (), false );
        EXPECT_EQ ( cval . count (), 2U );
        EXPECT_EQ ( cval . exists ( 0 ), true );
        EXPECT_EQ ( cval . exists ( 1 ), true );
        EXPECT_EQ ( cval . exists ( 2 ), false );
        EXPECT_EQ ( cval . getValue ( 0 ) . toInteger (), 123 );
        EXPECT_EQ ( cval . getValue ( 1 ) . toString (), String ( "abc" ) );
        EXPECT_ANY_THROW ( cval . getValue ( 2 ) );
        EXPECT_EQ ( cval . toJSON (), String ( "[123,\"abc\"]" ) );

#if ALLOW_MUTABLE_GET_VALUE_TO_INSERT_NULLS
        EXPECT_NO_THROW ( val [ 5 ] = true );
        EXPECT_EQ ( cval . toJSON (), String ( "[123,\"abc\",null,null,null,true]" ) );
#endif

        val [ 0 ] = nullptr;
        //val [ 1 ] = 456;

    }

    /*=================================================*
     *                      JSON                       *
     *=================================================*/

    TEST ( JSONParseTest, empty_input )
    {
        EXPECT_THROW ( JSON :: parse ( "" ), MalformedJSON );
    }

    TEST ( JSONParseTest, expect_right_brace )
    {
        EXPECT_THROW ( JSON :: parse ( "{" ), MalformedJSON );
    }

    TEST ( JSONParseTest, expect_left_brace )
    {
        EXPECT_THROW ( JSON :: parse ( "}" ), MalformedJSON );
    }

    TEST ( JSONParseTest, expect_colon )
    {
        EXPECT_THROW ( JSON :: parse ( "{\"name\"\"value\"" ), MalformedJSON );
    }

    TEST ( JSONParseTest, expect_right_brace_2 )
    {
        EXPECT_THROW ( JSON :: parse ( "{\"name\":\"value\"" ), MalformedJSON );
    }

    TEST ( JSONParseTest, parse_empty_object )
    {
        JSONValueRef val;
        EXPECT_NO_THROW ( val = JSON :: parse ( " { } " ) );
        EXPECT_EQ ( ! val, false );
    }

    TEST ( JSONParseTest, parse_object )
    {
        JSONValueRef val;
        EXPECT_NO_THROW ( val = JSON :: parse ( "{\"name1\":\"value\", \"name2\":\":value\"}" ) );
        EXPECT_EQ ( ! val, false );
    }

    TEST ( JSONParseTest, parse_object_similar_names )
    {
        JSONValueRef val;
        EXPECT_NO_THROW ( val = JSON :: parse ( "{\"nameA\":\"value\", \"name\":\":value\"}" ) );
        EXPECT_EQ ( ! val, false );
    }

    TEST ( JSONParseTest, parse_object_duplicate_names )
    {
        EXPECT_THROW (
            JSON :: parse ( "{\"name\":\"value\", \"name\":\":value\"}" ),
            JSONUniqueConstraintViolation );
    }

    TEST ( JSONParseTest, parse_object_bad_unicode_name )
    {
        EXPECT_THROW (
            JSON :: parse ( "{\"\\uDFAA\":0}" ),
            InvalidUTF16String );
    }

    TEST ( JSONParseTest, parse_array_expect_right_bracket )
    {
        EXPECT_THROW ( JSON :: parse ( "[}" ), MalformedJSON );
    }

    TEST ( JSONParseTest, parse_array_expect_right_bracket_2 )
    {
        EXPECT_THROW ( JSON :: parse ( "[\"name\",\"name\"" ), MalformedJSON );
    }

    TEST ( JSONParseTest, parse_array_expect_left_bracket )
    {
        EXPECT_THROW ( JSON :: parse ( "\"name\",\"name\"]" ), MalformedJSON );
    }

    TEST ( JSONParseTest, parse_array_empty )
    {
        JSONValueRef val;
        EXPECT_NO_THROW ( val = JSON :: parse ( "[]" ) );
        EXPECT_EQ ( ! val, false );
    }

    TEST ( JSONParseTest, parse_array_string_elems )
    {
        JSONValueRef val;
        EXPECT_NO_THROW ( val = JSON :: parse ( "[\"name\",\"name\"]" ) );
        EXPECT_EQ ( ! val, false );
    }

    TEST ( JSONParseTest, parse_array_parse_bison_leak )
    {
        JSONValueRef val;
        EXPECT_NO_THROW ( val = JSON :: parse ( "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]" ) );
        EXPECT_EQ ( ! val, false );
    }

    TEST ( JSONParseTest, parse_string_simple )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "\"abcd\"" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_EQ ( val . toString (), String ( "abcd" ) );
    }

    TEST ( JSONParseTest, parse_string_escapes )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "\" \\\" \\\\ \\/ \\b \\f \\n \\r \\t \"" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_EQ ( val . toString (), String ( " \" \\ / \b \f \n \r \t " ) );
    }

    TEST ( JSONParseTest, parse_utf16_to_utf8 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "\"\\u0441\\u0442\\u0440\\u0430\\u043d\\u043d\\u044b\\u0435\"" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_EQ ( val . toString (), String ( "странные" ) ); // hah!
    }

    TEST ( JSONParseTest, parse_surrogate_pair_to_utf8 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "\"\\udbff\\udfff\"" ) ); // U+10FFFF
        EXPECT_EQ ( ! ref, false );

        // NB - performing a raw string comparison to guard against any symmetrical
        // problems introduced by String conversion from UTF-8 to internal.
        const JSONValue & val = * ref;
        EXPECT_STREQ ( NULTerminatedString ( val . toString () ) . c_str (), "\xf4\x8f\xbf\xbf" );
    }

    TEST ( JSONParseTest, parse_bad_surrogate_pair_1 )
    {
        EXPECT_THROW ( JSON :: parse ( "{\"\" : \"\\udbff\" }" ), InvalidUTF16String );
    }

    TEST ( JSONParseTest, parse_bad_surrogate_pair_2 )
    {
        EXPECT_THROW ( JSON :: parse ( "{\"\" : \"\\udbff123456\" }" ), InvalidUTF16String );
    }

    TEST ( JSONParseTest, parse_bad_surrogate_pair_3 )
    {
        EXPECT_THROW ( JSON :: parse ( "{\"\" : \"\\udbff\\t\" }" ), InvalidUTF16String );
    }

    TEST ( JSONParseTest, parse_bad_surrogate_pair_4 )
    {
        EXPECT_THROW ( JSON :: parse ( "{\"\" : \"\\udbff\\u123\" }" ), MalformedJSON );
    }

    TEST ( JSONParseTest, parse_bad_surrogate_pair_5 )
    {
        EXPECT_THROW ( JSON :: parse ( "{\"\" : \"\\udbff\\uCC00\" }" ), InvalidUTF16String );
    }

    TEST ( JSONParseTest, parse_bad_surrogate_pair_6 )
    {
        EXPECT_THROW ( JSON :: parse ( "{\"\" : \"\\udbff\\uE000\" }" ), InvalidUTF16String );
    }

    TEST ( JSONParseTest, parse_not_an_integer )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "0.1" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_THROW ( val . toInteger (), JSONIncompatibleType );
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_unsigned_integer )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "9223372036854775807" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toInteger () );
    }

    TEST ( JSONParseTest, parse_unsigned_integer_overflow )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "9223372036854775808" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_THROW ( val . toInteger (), JSONIncompatibleType );
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_signed_integer )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "-9223372036854775808" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toInteger () );
    }

    TEST ( JSONParseTest, parse_signed_integer_overflow )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "-9223372036854775809" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_THROW ( val . toInteger (), JSONIncompatibleType );
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_0 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "0" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_1 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "1" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_0_1 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "0.1" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_1_1 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "1.1" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_1e1 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "1e1" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_1E1 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "1E1" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_1ep1 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "1e+1" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_1em1 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "1e-1" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_1_2e3 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "1.2e3" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_1_8ep308 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "1.8e+308" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
    }

    TEST ( JSONParseTest, parse_number_2_1em324 )
    {
        JSONValueRef ref;
        EXPECT_NO_THROW ( ref = JSON :: parse ( "2.1e-324" ) );
        EXPECT_EQ ( ! ref, false );

        const JSONValue & val = * ref;
        EXPECT_NO_THROW ( val . toNumber () );
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
