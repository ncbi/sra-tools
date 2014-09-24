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

#include "token.h"
#include "prefs-priv.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>


/* conversion functions
 *  returns false if there were problems, e.g. integer overflow
 *  returns a typed value in "val" on success
 *  "text" and "len" are taken from a volatile text buffer
 */
bool toolkit_dtoi ( uint64_t * val, const char *text, int len )
{
    uint64_t num;
    int i, cnt = len - 1;

    /* skip over leading zeros, leaving at least one digit */
    for ( i = 0; i < cnt; ++ i )
    {
        if ( text [ i ] != '0' )
            break;
    }

    /* initialize to left-most non-zero digit or zero */
    num = text [ i ] - '0';

    /* process remaining digits */
    for ( ++ i; i < len; ++ i )
    {
        uint64_t dig = text [ i ] - '0';

        /* test for overflow when scaling by 10 */
        if ( num > ( UINT64_MAX / 10 ) )
            return false;
        num *= 10;

        /* test for overflow when adding in new digit */
        if ( num > ( UINT64_MAX - dig ) )
            return false;
        num += dig;
    }

    /* return 64-bit unsigned value */
    * val = num;
    return true;
}

#undef VERS
#define VERS( M, m, r, b ) \
    ( ( ( ver_t ) ( M ) << 24 ) | \
      ( ( ver_t ) ( m ) << 16 ) | \
      ( ( ver_t ) ( r ) <<  8 ) | \
      ( ( ver_t ) ( b ) <<  0) )

bool toolkit_atov ( ver_t *val, const char *text, int len )
{
    int i, part;

    uint32_t num [ 4 ];
    num [ 0 ] = num [ 1 ] = num [ 2 ] = num [ 3 ] = 0;

    for ( i = part = 0; i < len; ++ i )
    {
        if ( text [ i ] == '.' )
            ++ part;
        else
        {
            num [ part ] *= 10;
            num [ part ] += text [ i ] - '0';
            if ( num [ part ] >= 256 )
            {
                * val = 0;
                return false;
            }
        }
    }

    * val = VERS ( num [ 0 ], num [ 1 ], num [ 2 ], num [ 3 ] );
    return true;
}

bool toolkit_atotm ( KTime_t *val, const char *text, int len )
{
    char *end;
    struct tm tm;

    /* must fit format of YYYY-MM-DDThh:mm:ssZ
       or potentially with time zone */
    if ( len < 20 )
        return false;

    /* get year 1900..2999 */
    tm . tm_year = ( int ) strtoul ( & text [ 0 ], & end, 10 );
    assert ( end == text + 4 );
    assert ( end [ 0 ] == '-' );
    if ( tm . tm_year < 1900 || tm . tm_year >= 3000 )
        return false;

    /* get month 1..12 */
    tm . tm_mon = ( int ) strtoul ( & text [ 5 ], & end, 10 );
    assert ( end == text + 7 );
    assert ( end [ 0 ] == '-' );
    if ( tm . tm_mon < 1 || tm . tm_mon > 12 )
        return false;

    /* get day 1..31 */
    tm . tm_mday = ( int ) strtoul ( & text [ 8 ], & end, 10 );
    assert ( end == text + 10 );
    assert ( toupper ( end [ 0 ] ) == 'T' );
    if ( tm . tm_mday < 1 || tm . tm_mday > 31 )
        return false;

    /* get hour 0..23 */
    tm . tm_hour = ( int ) strtoul ( & text [ 11 ], & end, 10 );
    assert ( end == text + 13 );
    assert ( end [ 0 ] == ':' );
    if ( tm . tm_hour > 23 )
        return false;

    /* get min 0..59 */
    tm . tm_min = ( int ) strtoul ( & text [ 14 ], & end, 10 );
    assert ( end == text + 16 );
    assert ( end [ 0 ] == ':' );
    if ( tm . tm_min > 59 )
        return false;

    /* get sec 0..61 - for leap seconds */
    tm . tm_sec = ( int ) strtoul ( & text [ 17 ], & end, 10 );
    assert ( end == text + 19 );
    assert ( toupper ( end [ 0 ] ) == 'Z' );
    if ( tm . tm_sec > 61 )
        return false;

    /* adjust for Unix mktime */
    tm . tm_year -= 1900;
    tm . tm_mon -= 1;
    * val = mktime ( & tm );
    return * val != -1;
}

/* rewrite token 
 * convert integer to ver_t
 * integer is in t -> val . u
 * and we can accept value 0 .. 255 anything else is an error
 * maj only
 */
Token toolkit_itov ( const Token *i )
{
    Token t = *i;

    if ( i -> val . u > 255 )
    {
        prefs_token_error ( i, "version component overflow" );
        t . val . u = 0;
    }
    else
    {
        t . val . v = VERS ( i -> val . u, 0, 0 , 0 );
        t . type = val_vers;
    }

    return t;
}

/* rewrite token
 * real number to ver_t
 * real number is a string in t -> val . c
 * we perform two integer conversions from ASCII
 * split on '.' 
 * maj . min
 */
Token toolkit_rtov ( const Token *r )
{
    Token t = *r;

    char *dot;
    unsigned long maj, min;

    assert ( r -> type == val_txt );

    /* we know that the string being used in the transformation is '.' 
     * in between integers because of the lex parser */
    maj = strtoul ( r -> val . c, & dot, 10 ); 
    min = strtoul ( dot + 1, NULL, 10 );

    if ( maj > 255 || min > 255 )
    {
        prefs_token_error ( r, "version component overflow" );
        t . val . u = 0;
    }
    else
    {
        t . val . v = VERS ( maj, min, 0, 0 );
        t . type = val_vers;
    }
 
    return t;
}
