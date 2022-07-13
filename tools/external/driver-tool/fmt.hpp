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

#pragma once

#include <ncbi/secure/string.hpp>
#define HAVE_SECURE_EXCEPT 1

#include <iostream>

#include <sys/types.h>

#ifndef HAVE_F128
 #if LINUX
  #define HAVE_F128 1
 #else
  #define HAVE_F128 0
 #endif
#endif


namespace ncbi
{

    /*-------------------------------------------------------------------
     * FmtWriter
     */
    struct FmtWriter
    {
        virtual void write ( const char * data, size_t bytes ) = 0;
        virtual size_t flush ( bool no_throw )
        { return 0; }

        // mostly for debugging
        void print ( const char * msg, ... );

        virtual ~ FmtWriter () {}
    };

    /*-------------------------------------------------------------------
     * Fmt
     */
    class Fmt
    {
    public:

        Fmt & operator << ( bool val );
        inline Fmt & operator << ( short val )
        { return * this << ( long long ) val; }
        inline Fmt & operator << ( unsigned short val )
        { return * this << ( unsigned long long ) val; }
        inline Fmt & operator << ( int val )
        { return * this << ( long long ) val; }
        inline Fmt & operator << ( unsigned int val )
        { return * this << ( unsigned long long ) val; }
        inline Fmt & operator << ( long val )
        { return * this << ( long long ) val; }
        inline Fmt & operator << ( unsigned long val )
        { return * this << ( unsigned long long ) val; }
        Fmt & operator << ( long long val );
        Fmt & operator << ( unsigned long long val );
        Fmt & operator << ( __int128 val );
        Fmt & operator << ( unsigned __int128 val );
        inline Fmt & operator << ( float val )
        { return * this << ( double ) val; }
        Fmt & operator << ( double val );
#if HAVE_F128
        inline Fmt & operator << ( long double val )
        { return * this << ( __float128 ) val; }
        Fmt & operator << ( __float128 val );
#else
        Fmt & operator << ( long double val );
#endif
        Fmt & operator << ( const void * val );
        Fmt & operator << ( Fmt & ( * f ) ( Fmt & f ) );

        void put ( unsigned char c );
        void cstr ( const char * str );
        void write ( const void * data, size_t bytes );
        void setRadix ( unsigned int radix );
        void setWidth ( unsigned int width );
        void setPrecision ( unsigned int precision );
        void setFill ( char fill );
        void indent ( int num_tabs );
        void sysError ( int status );

        bool atLineStart () const
        { return at_line_start; }

        Fmt ( FmtWriter & writer, bool add_eoln = false, bool use_crlf = false );
        ~ Fmt ();

    private:

        void resize ( size_t increase );
        void indent ();

        FmtWriter & writer;
        char * buffer;
        size_t bsize;
        size_t marker;
        unsigned short radix;
        unsigned short width;
        unsigned short precision;
        unsigned short indentation;
        unsigned char tabwidth;
        char fill;
        bool have_precision;
        bool at_line_start;
        bool add_eoln;
        bool use_crlf;

        friend Fmt & eoln ( Fmt & f );
        friend Fmt & endm ( Fmt & f );
        friend Fmt & flushm ( Fmt & f );
    };

    // control indentation
    Fmt & ind ( Fmt & f );
    Fmt & outd ( Fmt & f );

    // include a newline
    Fmt & eoln ( Fmt & f );

    // this is how to indicate end of format message
    // will automatically terminate with eoln if "add_eoln" is true
    Fmt & endm ( Fmt & f );

    // this is how to indicate flushing
    Fmt & flushm ( Fmt & f );

    
    // somehow, placing these as global operators helps
    // with C++ ostream, so I won't argue
    inline Fmt & operator << ( Fmt & f, char c )
    { f . put ( c ); return f; }
    inline Fmt & operator << ( Fmt & f, signed char c )
    { f . put ( c ); return f; }
    inline Fmt & operator << ( Fmt & f, unsigned char c )
    { f . put ( c ); return f; }

    inline Fmt & operator << ( Fmt & f, const char * p )
    { f . cstr ( p ); return f; }
    inline Fmt & operator << ( Fmt & f, const signed char * p )
    { f . cstr ( ( const char * ) p ); return f; }
    inline Fmt & operator << ( Fmt & f, const unsigned char * p )
    { f . cstr ( ( const char * ) p ); return f; }

    inline Fmt & operator << ( Fmt & f, const String & str )
    { f . write ( str . data (), str . size () ); return f; }
    inline Fmt & operator << ( Fmt & f, const std :: string & str )
    { f . write ( str . data (), str . size () ); return f; }

    // C++ style hacks to set width and fill character
    struct FmtWidth { unsigned short width; };
    inline FmtWidth setw ( int w )
    { FmtWidth r; r . width = w; return r; }
    inline Fmt & operator << ( Fmt & f, const FmtWidth & w )
    { f . setWidth ( w . width ); return f; }

    struct FmtFill { char fill; };
    inline FmtFill setf ( char c )
    { FmtFill r; r . fill = c; return r; }
    inline Fmt & operator << ( Fmt & f, const FmtFill & c )
    { f . setFill ( c . fill ); return f; }

    struct FmtPrec { unsigned short prec; };
    inline FmtPrec setp ( unsigned short p )
    { FmtPrec r; r . prec = p; return r; }
    inline Fmt & operator << ( Fmt & f, const FmtPrec & p )
    { f . setPrecision ( p . prec ); return f; }

#if HAVE_SECURE_EXCEPT
    typedef XPRadix FmtRadix;
#else
    struct FmtRadix { unsigned short radix; };
#endif
    inline FmtRadix binary ()
    { FmtRadix r; r . radix = 2; return r; }
    inline FmtRadix octal ()
    { FmtRadix r; r . radix = 8; return r; }
    inline FmtRadix decimal ()
    { FmtRadix r; r . radix = 10; return r; }
    inline FmtRadix hex ()
    { FmtRadix r; r . radix = 16; return r; }
#if ! HAVE_SECURE_EXCEPT
    inline FmtRadix radix ( unsigned int val )
    { FmtRadix r; r . radix = ( unsigned short ) val; return r; }
#endif
    inline Fmt & operator << ( Fmt & f, const FmtRadix & r )
    { f . setRadix ( r . radix ); return f; }

    struct FmtIndent { int amt; };
    inline FmtIndent indent ( int amt )
    { FmtIndent i; i . amt = amt; return i; }
    inline Fmt & operator << ( Fmt & f, const FmtIndent & i )
    { f . indent ( i . amt ); return f; }

#if HAVE_SECURE_EXCEPT
    typedef XPSysErr FmtErrno;
#else
    struct FmtErrno { int status; };
    inline FmtErrno syserr ( int status )
    { FmtErrno r; r . status = status; return r; }
#endif
    inline Fmt & operator << ( Fmt & f, const FmtErrno & e )
    { f . sysError ( e . status ); return f; }

#if HAVE_SECURE_EXCEPT
    //!< support for XMsg and std::ostream
    inline Fmt & operator << ( Fmt & f, const XMsg & m )
    { return f << m . zmsg; }

    //!< support for XBackTrace and std::ostream
    Fmt & operator << ( Fmt & f, const XBackTrace & bt );
#endif
}
