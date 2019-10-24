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

#include "fmt.hpp"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#if HAVE_F128
#include <quadmath.h>
#endif

namespace ncbi
{

    static
    bool vprint_use_crlf ( const char * fmt )
    {
        size_t size = strlen ( fmt );
        return ( size > 1 && fmt [ size - 2 ] == '\r' && fmt [ size - 1 ] == '\n' );
    }

    static
    void vprint ( FmtWriter & writer, const char * fmt, va_list args )
    {
        size_t bytes;
        char buffer [ 4096 ];
        const char * msg = buffer;

        int status = vsnprintf ( buffer, sizeof buffer, fmt, args );
        bytes = status;

        if ( status < 0 )
        {
            bool use_crlf = vprint_use_crlf ( fmt );
            if ( use_crlf )
            {
                const char too_long [] = "message too long...\r\n";
                msg = too_long;
                bytes = sizeof too_long - 1;
            }
            else
            {
                const char too_long [] = "message too long...\n";
                msg = too_long;
                bytes = sizeof too_long - 1;
            }
        }
        else if ( bytes > sizeof buffer )
        {
            bool use_crlf = vprint_use_crlf ( fmt );
            bytes = sizeof buffer;
            if ( use_crlf )
                memcpy ( & buffer [ sizeof buffer - 5 ], "...\r\n", 5 );
            else
                memcpy ( & buffer [ sizeof buffer - 4 ], "...\n", 4 );
        }

        writer . write ( msg, bytes );
    }

    void FmtWriter :: print ( const char * fmt, ... )
    {
        va_list args;
        va_start ( args, fmt );

        vprint ( * this, fmt, args );

        va_end ( args );
    }

    Fmt & Fmt :: operator << ( bool val )
    {
        if ( val )
            write ( "true", 4 );
        else
            write ( "false", 5 );

        return * this;
    }

    Fmt & Fmt :: operator << ( long long sval )
    {
        // since we print directly into the buffer,
        // perform our own indentation
        if ( at_line_start && indentation > 0 )
            indent ();

        unsigned long long val = sval;

        bool neg = false;
        if ( sval < 0 )
        {
            neg = true;
            val = - sval;
        }

        char digits [ sizeof val * 8 + 3 ];

        unsigned int i = sizeof digits;
        do
        {
            digits [ -- i ] = "0123456789abcdefhijklmnopqrstuvwxyz" [ val % radix ];
            val /= radix;
        }
        while ( val != 0 );

        // the number of actual digits
        unsigned int cur_width = sizeof digits - i;

        // figure in sign and prefix if not base 10
        unsigned int total_width = cur_width + neg + 2 * ( radix != 10 );

        // calculate the amount of padding
        unsigned int padding = 0;
        if ( total_width < ( unsigned int ) width )
            padding = ( unsigned int ) width - total_width;

        // the total field width
        total_width += padding;

        // test against what is available
        if ( marker + total_width > bsize )
            resize ( marker + total_width - bsize );

        // padd with spaces
        if ( padding != 0 && fill == ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // set sign
        if ( neg )
            buffer [ marker ++ ] = '-';

        // handle prefix
        switch ( radix )
        {
        case 10:
            break;
        case 2:
            memcpy ( & buffer [ marker ], "0b", 2 );
            marker += 2;
            break;
        case 8:
            memcpy ( & buffer [ marker ], "0o", 2 );
            marker += 2;
            break;
        case 16:
            memcpy ( & buffer [ marker ], "0x", 2 );
            marker += 2;
            break;
        }

        // padd with zeros
        if ( padding != 0 && fill != ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // now finish with digits
        memcpy ( & buffer [ marker ], & digits [ i ], cur_width );
        marker += cur_width;

        // in wonderful C++ fashion, reset everything
        radix = 10;
        width = 0;
        precision = 0;
        fill = ' ';
        have_precision = false;
        at_line_start = false;

        return * this;
    }

    Fmt & Fmt :: operator << ( unsigned long long val )
    {
        // since we print directly into the buffer,
        // perform our own indentation
        if ( at_line_start && indentation > 0 )
            indent ();

        char digits [ sizeof val * 8 + 2 ];

        unsigned int i = sizeof digits;
        do
        {
            digits [ -- i ] = "0123456789abcdefhijklmnopqrstuvwxyz" [ val % radix ];
            val /= radix;
        }
        while ( val != 0 );

        // the number of actual digits
        unsigned int cur_width = sizeof digits - i;

        // figure in prefix if not base 10
        unsigned int total_width = cur_width + 2 * ( radix != 10 );

        // calculate the amount of padding
        unsigned int padding = 0;
        if ( total_width < ( unsigned int ) width )
            padding = ( unsigned int ) width - total_width;

        // the total field width
        total_width += padding;

        // test against what is available
        if ( marker + total_width > bsize )
            resize ( marker + total_width - bsize );

        // padd with spaces
        if ( padding != 0 && fill == ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // handle prefix
        switch ( radix )
        {
        case 10:
            break;
        case 2:
            memcpy ( & buffer [ marker ], "0b", 2 );
            marker += 2;
            break;
        case 8:
            memcpy ( & buffer [ marker ], "0o", 2 );
            marker += 2;
            break;
        case 16:
            memcpy ( & buffer [ marker ], "0x", 2 );
            marker += 2;
            break;
        }

        // padd with zeros
        if ( padding != 0 && fill != ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // now finish with digits
        memcpy ( & buffer [ marker ], & digits [ i ], cur_width );
        marker += cur_width;

        // in wonderful C++ fashion, reset everything
        radix = 10;
        width = 0;
        precision = 0;
        fill = ' ';
        have_precision = false;
        at_line_start = false;

        return * this;
    }

    Fmt & Fmt :: operator << ( __int128 sval )
    {
        unsigned __int128 val = sval;

        bool neg = false;
        if ( sval < 0 )
        {
            neg = true;
            val = - sval;
        }

#if 0
        // test for being within 64-bit range
        if ( ( val >> 64 ) == 0 )
            return operator << ( long ) sval;
#endif
        // since we print directly into the buffer,
        // perform our own indentation
        if ( at_line_start && indentation > 0 )
            indent ();

        char digits [ sizeof val * 8 + 3 ];

        unsigned int i = sizeof digits;
        do
        {
            digits [ -- i ] = "0123456789abcdefhijklmnopqrstuvwxyz" [ val % radix ];
            val /= radix;
        }
        while ( val != 0 );

        // the number of actual digits
        unsigned int cur_width = sizeof digits - i;

        // figure in sign and prefix if not base 10
        unsigned int total_width = cur_width + neg + 2 * ( radix != 10 );

        // calculate the amount of padding
        unsigned int padding = 0;
        if ( total_width < ( unsigned int ) width )
            padding = ( unsigned int ) width - total_width;

        // the total field width
        total_width += padding;

        // test against what is available
        if ( marker + total_width > bsize )
            resize ( marker + total_width - bsize );

        // padd with spaces
        if ( padding != 0 && fill == ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // set sign
        if ( neg )
            buffer [ marker ++ ] = '-';

        // handle prefix
        switch ( radix )
        {
        case 10:
            break;
        case 2:
            memcpy ( & buffer [ marker ], "0b", 2 );
            marker += 2;
            break;
        case 8:
            memcpy ( & buffer [ marker ], "0o", 2 );
            marker += 2;
            break;
        case 16:
            memcpy ( & buffer [ marker ], "0x", 2 );
            marker += 2;
            break;
        }

        // padd with zeros
        if ( padding != 0 && fill != ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // now finish with digits
        memcpy ( & buffer [ marker ], & digits [ i ], cur_width );
        marker += cur_width;

        // in wonderful C++ fashion, reset everything
        radix = 10;
        width = 0;
        precision = 0;
        fill = ' ';
        have_precision = false;
        at_line_start = false;

        return * this;
    }

    Fmt & Fmt :: operator << ( unsigned __int128 val )
    {
#if 0
        // test for being within 64-bit range
        if ( ( val >> 64 ) == 0 )
            return operator << ( unsigned long ) val;
#endif
        // since we print directly into the buffer,
        // perform our own indentation
        if ( at_line_start && indentation > 0 )
            indent ();

        char digits [ sizeof val * 8 + 2 ];

        unsigned int i = sizeof digits;
        do
        {
            digits [ -- i ] = "0123456789abcdefhijklmnopqrstuvwxyz" [ val % radix ];
            val /= radix;
        }
        while ( val != 0 );

        // the number of actual digits
        unsigned int cur_width = sizeof digits - i;

        // figure in prefix if not base 10
        unsigned int total_width = cur_width + 2 * ( radix != 10 );

        // calculate the amount of padding
        unsigned int padding = 0;
        if ( total_width < ( unsigned int ) width )
            padding = ( unsigned int ) width - total_width;

        // the total field width
        total_width += padding;

        // test against what is available
        if ( marker + total_width > bsize )
            resize ( marker + total_width - bsize );

        // padd with spaces
        if ( padding != 0 && fill == ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // handle prefix
        switch ( radix )
        {
        case 10:
            break;
        case 2:
            memcpy ( & buffer [ marker ], "0b", 2 );
            marker += 2;
            break;
        case 8:
            memcpy ( & buffer [ marker ], "0o", 2 );
            marker += 2;
            break;
        case 16:
            memcpy ( & buffer [ marker ], "0x", 2 );
            marker += 2;
            break;
        }

        // padd with zeros
        if ( padding != 0 && fill != ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // now finish with digits
        memcpy ( & buffer [ marker ], & digits [ i ], cur_width );
        marker += cur_width;

        // in wonderful C++ fashion, reset everything
        radix = 10;
        width = 0;
        precision = 0;
        fill = ' ';
        have_precision = false;
        at_line_start = false;

        return * this;
    }

    Fmt & Fmt :: operator << ( double val )
    {
        // rely upon stdclib printf engine for the time being
        int cur_width = 0;
        char digits [ 512 ];

        if ( have_precision )
            cur_width = snprintf ( digits, sizeof digits, "%.*lf", precision, val );
        else
            cur_width = snprintf ( digits, sizeof digits, "%lf", val );

        if ( cur_width < 0 || ( size_t ) cur_width >= sizeof digits )
        {
            write ( "<bad-value>", sizeof "<bad-value>" - 1 );
            return * this;
        }

        // since we print directly into the buffer,
        // perform our own indentation
        if ( at_line_start && indentation > 0 )
            indent ();

        // recover sign
        bool neg = digits [ 0 ] == '-';
        size_t i = neg;
        cur_width -= neg;

        // figure in sign and prefix if not base 10
        unsigned int total_width = cur_width + neg;

        // calculate the amount of padding
        unsigned int padding = 0;
        if ( total_width < ( unsigned int ) width )
            padding = ( unsigned int ) width - total_width;

        // the total field width
        total_width += padding;

        // test against what is available
        if ( marker + total_width > bsize )
            resize ( marker + total_width - bsize );

        // padd with spaces
        if ( padding != 0 && fill == ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // set sign
        if ( neg )
            buffer [ marker ++ ] = '-';

        // padd with zeros
        if ( padding != 0 && fill != ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // now finish with digits
        memcpy ( & buffer [ marker ], & digits [ i ], cur_width );
        marker += cur_width;

        // in wonderful C++ fashion, reset everything
        radix = 10;
        width = 0;
        precision = 0;
        fill = ' ';
        have_precision = false;
        at_line_start = false;

        return * this;
    }

#if HAVE_F128
    Fmt & Fmt :: operator << ( __float128 val )
#else
    Fmt & Fmt :: operator << ( long double val )
#endif
    {
        // rely upon stdclib printf engine for the time being
        int cur_width = 0;
        char digits [ 4096 ];

#if HAVE_F128
        if ( have_precision )
            cur_width = quadmath_snprintf ( digits, sizeof digits, "%.*f", precision, val );
        else
            cur_width = quadmath_snprintf ( digits, sizeof digits, "%f", val );
#else
        if ( have_precision )
            cur_width = snprintf ( digits, sizeof digits, "%.*Lf", precision, val );
        else
            cur_width = snprintf ( digits, sizeof digits, "%Lf", val );
#endif

        if ( cur_width < 0 || ( size_t ) cur_width >= sizeof digits )
        {
            write ( "<bad-value>", sizeof "<bad-value>" - 1 );
            return * this;
        }

        // since we print directly into the buffer,
        // perform our own indentation
        if ( at_line_start && indentation > 0 )
            indent ();

        // recover sign
        bool neg = digits [ 0 ] == '-';
        size_t i = neg;
        cur_width -= neg;

        // figure in sign and prefix if not base 10
        unsigned int total_width = cur_width + neg;

        // calculate the amount of padding
        unsigned int padding = 0;
        if ( total_width < ( unsigned int ) width )
            padding = ( unsigned int ) width - total_width;

        // the total field width
        total_width += padding;

        // test against what is available
        if ( marker + total_width > bsize )
            resize ( marker + total_width - bsize );

        // padd with spaces
        if ( padding != 0 && fill == ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // set sign
        if ( neg )
            buffer [ marker ++ ] = '-';

        // padd with zeros
        if ( padding != 0 && fill != ' ' )
        {
            memset ( & buffer [ marker ], fill, padding );
            marker += padding;
        }

        // now finish with digits
        memcpy ( & buffer [ marker ], & digits [ i ], cur_width );
        marker += cur_width;

        // in wonderful C++ fashion, reset everything
        radix = 10;
        width = 0;
        precision = 0;
        fill = ' ';
        have_precision = false;
        at_line_start = false;

        return * this;
    }

    Fmt & Fmt :: operator << ( const void * val )
    {
        return * this
            << hex ()
            << ( unsigned long ) val
            ;
    }

    Fmt & Fmt :: operator << ( Fmt & ( * f ) ( Fmt & f ) )
    {
        return ( * f ) ( * this );
    }

    void Fmt :: put ( unsigned char c )
    {
        // look for need to use cr-lf
        if ( c == '\n' )
        {
            if ( use_crlf )
            {
                // resize buffer to accept single-byte character
                if ( marker == bsize )
                    resize ( 1 );

                // add CR
                buffer [ marker ++ ] = '\r';
            }

            at_line_start = true;
        }

        // since we print directly into the buffer,
        // perform our own indentation, but
        // only if not printing a newline already
        else if ( at_line_start && indentation > 0 )
            indent ();

        // resize buffer to accept single-byte character
        if ( marker == bsize )
            resize ( 1 );

        // add character
        buffer [ marker ++ ] = c;
    }

    void Fmt :: cstr ( const char * str )
    {
        if ( str == 0 )
            str = "<null>";

        write ( str, strlen ( str ) );
    }

    void Fmt :: write ( const void * data, size_t bytes )
    {
        if ( data != 0 && bytes != 0 )
        {
            bool still_at_line_start = false;

            // since we print directly into the buffer,
            // perform our own indentation, but
            // only if not printing a newline already
            if ( at_line_start && indentation > 0 )
            {
                const char * text = ( const char * ) data;
                switch ( bytes )
                {
                case 1:
                    if ( text [ 0 ] == '\n' )
                        break;
                    else
                    {
                case 2:
                    if ( text [ 0 ] == '\r' && text [ 1 ] == '\n' )
                        break;
                    }
                    // no break;
                default:
                    indent ();
                }

                still_at_line_start = at_line_start;
            }

            if ( marker + bytes > bsize )
                resize ( marker + bytes - bsize );

            memcpy ( & buffer [ marker ], data, bytes );
            marker += bytes;
            at_line_start = still_at_line_start;
        }
    }

    void Fmt :: setRadix ( unsigned int _radix )
    {
        assert ( _radix > 1 && _radix <= 36 );
        radix = ( unsigned short ) _radix;
    }

    void Fmt :: setWidth ( unsigned int _width )
    {
        assert ( _width < 0x10000 );
        width = ( unsigned short ) _width;
    }

    void Fmt :: setPrecision ( unsigned int _precision )
    {
        assert ( _precision < 0x10000 );
        precision = ( unsigned short ) _precision;
        have_precision = true;
    }

    void Fmt :: setFill ( char _fill )
    {
        assert ( _fill == ' ' || _fill == '0' );
        fill = _fill;
    }

    void Fmt :: indent ( int num_tabs )
    {
        indentation += num_tabs;
        if ( indentation < 0 )
            indentation = 0;
    }

    void Fmt :: sysError ( int status )
    {
        StringBuffer msg;
        msg += :: strerror ( status );
        write ( msg . data (), msg . size () );
        const char ERRNO [] = " ( errno = ";
        write ( ERRNO, sizeof ERRNO - 1 );
        * this << ( long ) status;
        write ( " )", 2 );
    }

    Fmt :: Fmt ( FmtWriter & _writer, bool _add_eoln, bool _use_crlf )
        : writer ( _writer )
        , buffer ( 0 )
        , bsize ( 0 )
        , marker ( 0 )
        , radix ( 10 )
        , width ( 0 )
        , precision ( 0 )
        , indentation ( 0 )
        , tabwidth ( 4 )
        , fill ( ' ' )
        , have_precision ( false )
        , at_line_start ( true )
        , add_eoln ( _add_eoln )
        , use_crlf ( _use_crlf )
    {
    }

    Fmt :: ~ Fmt ()
    {
        delete [] buffer;
        buffer = 0;
        marker = bsize = 0;
    }

    void Fmt :: resize ( size_t increase )
    {
        size_t new_size = ( bsize + increase + 4095 ) & ~ ( size_t ) 4095;
        char * new_buffer = new char [ new_size ];
        if ( buffer != 0 )
        {
            memcpy ( new_buffer, buffer, marker );
            delete [] buffer;
        }
        buffer = new_buffer;
        bsize = new_size;
    }

    void Fmt :: indent ()
    {
        size_t bytes = ( unsigned int ) indentation * tabwidth;

        if ( marker + bytes > bsize )
            resize ( marker + bytes - bsize );

        memset ( & buffer [ marker ], ' ', bytes );
        marker += bytes;

        at_line_start = false;
    }

    Fmt & ind ( Fmt & f )
    {
        f . indent ( 1 );
        return f;
    }

    Fmt & outd ( Fmt & f )
    {
        f . indent ( -1 );
        return f;
    }

    Fmt & eoln ( Fmt & f )
    {
        f . put ( '\n' );
        assert ( f . at_line_start == true );
        return f;
    }

    Fmt & endm ( Fmt & f )
    {
        const char * data = f . buffer;
        size_t bytes = f . marker;

        f . indentation = 0;

        if ( f . add_eoln )
        {
            if ( bytes > 0 && data [ bytes - 1 ] != '\n' )
            {
                f . put ( '\n' );

                data = f . buffer;
                bytes = f . marker;
            }
        }

        f . marker = 0;
        f . radix = 10;
        f . width = 0;
        f . precision = 0;
        f . tabwidth = 4;
        f . fill = ' ';
        f . have_precision = false;
        f . at_line_start = true;

        f . writer . write ( data, bytes );

        f . marker = 0;

        return f;
    }

    Fmt & flushm ( Fmt & f )
    {
        if ( f . marker != 0 )
        {
            f . writer . write ( f . buffer, f . marker );
            f . marker = 0;
        }

        f . writer . flush ( false );
        return f;
    }

#if HAVE_SECURE_EXCEPT
    Fmt & operator << ( Fmt & f, const XBackTrace & bt )
    {
        if ( bt . isValid () )
        {
            do
            {
                f
                    << bt . getName ()
                    << '\n'
                    ;
            }
            while ( bt . up () );
        }

        return f;
    }
#endif
}
