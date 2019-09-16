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

#include <ncbi/secure/except.hpp>
#include "utf8proc.h"
#include "memset-priv.hpp"

// sensitive to whatever crypto library is in use
#include <mbedtls/error.h>

#include <cstring>
#include <cstdarg>
#include <cxxabi.h>

#include <string.h>
#include <errno.h>
#include <execinfo.h>
#include <cassert>

namespace ncbi
{
    static
    XMsg copyXMsg ( const std :: string & s )
    {
        XMsg x;
        size_t bytes = s . size ();
        if ( bytes >= sizeof x . zmsg )
            bytes = sizeof x . zmsg - 1;
        memmove ( x . zmsg, s . data (), bytes );
        x . msg_size = bytes;
        x . zmsg [ bytes ] = 0;
        return x;
    }

    static
    XMsg copyXMsg ( const char * s )
    {
        XMsg x;
        size_t i;
        for ( i = 0; s [ i ] != 0 && i < sizeof x . zmsg - 1; ++ i )
            x . zmsg [ i ] = s [ i ];
        x . msg_size = i;
        x . zmsg [ i ] = 0;
        return x;
    }

    static
    XMsg & concatXMsg ( XMsg & x, const std :: string & s )
    {
        assert ( x . msg_size < sizeof x . zmsg );
        size_t bytes = s . size ();
        if ( x . msg_size + bytes >= sizeof x . zmsg )
            bytes = sizeof x . zmsg - x . msg_size - 1;
        memmove ( & x . zmsg [ x . msg_size ], s . data (), bytes );
        x . msg_size += bytes;
        x . zmsg [ x . msg_size ] = 0;
        return x;
    }

    static
    XMsg & concatXMsg ( XMsg & x, const char * s )
    {
        assert ( x . msg_size < sizeof x . zmsg );
        size_t i, remain = sizeof x . zmsg - x . msg_size - 1;
        for ( i = 0; s [ i ] != 0 && i < remain; ++ i )
            x . zmsg [ x . msg_size + i ] = s [ i ];
        x . msg_size += i;
        x . zmsg [ x . msg_size ] = 0;
        return x;
    }

    const XMsg Exception :: what () const noexcept
    {
        XMsg x = copyXMsg ( prob_msg );
        if ( ! ctx_msg . empty () )
            concatXMsg ( concatXMsg ( x, " while " ), ctx_msg );
        if ( ! cause_msg . empty () )
            concatXMsg ( concatXMsg ( x, ": " ), cause_msg );
        if ( ! suggest_msg . empty () )
            concatXMsg ( concatXMsg ( x, ". " ), suggest_msg );
        return x;
    }

    const XMsg Exception :: problem () const noexcept
    {
        return copyXMsg ( prob_msg );
    }

    const XMsg Exception :: context () const noexcept
    {
        return copyXMsg ( ctx_msg );
    }

    const XMsg Exception :: cause () const noexcept
    {
        return copyXMsg ( cause_msg );
    }

    const XMsg Exception :: suggestion () const noexcept
    {
        return copyXMsg ( suggest_msg );
    }

    const XMsg Exception :: file () const noexcept
    {
        return copyXMsg ( file_name );
    }

    unsigned int Exception :: line () const noexcept
    {
        return lineno;
    }

    const XMsg Exception :: function () const noexcept
    {
        return copyXMsg ( func_name );
    }

    ReturnCodes Exception :: status () const noexcept
    {
        return rc;
    }

    static
    const UTF8 * string_rchr ( const UTF8 * str, int ch, const UTF8 * from )
    {
        for ( size_t sz = from - str; sz > 0; )
        {
            if ( str [ -- sz ] == ch )
                return & str [ sz ];
        }

        return nullptr;
    }

    Exception & Exception :: operator = ( const Exception & x )
    {
        for ( int i = 0; i < x . stack_frames; ++ i )
            callstack [ i ] = x . callstack [ i ];
        stack_frames = x . stack_frames;

        prob_msg = x . prob_msg;
        ctx_msg = x . ctx_msg;
        cause_msg = x . cause_msg;
        suggest_msg = x . suggest_msg;
        file_name = x . file_name;
        func_name = x . func_name;
        lineno = x . lineno;
        rc = x . rc;

        return * this;
    }

    Exception :: Exception ( const Exception & x )
        : prob_msg ( x . prob_msg )
        , ctx_msg ( x . ctx_msg )
        , cause_msg ( x . cause_msg )
        , suggest_msg ( x . suggest_msg )
        , file_name ( x . file_name )
        , func_name ( x . func_name )
        , lineno ( x . lineno )
        , stack_frames ( x . stack_frames )
        , rc ( x . rc )
    {
        for ( int i = 0; i < x . stack_frames; ++ i )
            callstack [ i ] = x . callstack [ i ];
    }

    // the callstack captured by XP::XP() will have
    // itself as the first entry, so might skip it
    // for some reason, I see two of these... weird.
    const int num_stack_frames_to_skip = 2;

    Exception :: Exception ( const XP & p )
        : prob_msg ( p . problem )
        , ctx_msg ( p . context )
        , cause_msg ( p . cause )
        , suggest_msg ( p . suggestion )
        , file_name ( p . file_name )
        , func_name ( p . func_name )
        , lineno ( p . lineno )
        , stack_frames ( p . stack_frames - num_stack_frames_to_skip )
        , rc ( p . rc )
    {
        for ( int i = 0; i < stack_frames; ++ i )
            callstack [ i ] = p . callstack [ i + num_stack_frames_to_skip ];
    }

    Exception :: ~ Exception () noexcept
    {
        memset_while_respecting_language_semantics
            ( ( void * ) prob_msg . data (), prob_msg . capacity (),
              ' ', prob_msg . capacity (), prob_msg . data () );
        memset_while_respecting_language_semantics
            ( ( void * ) ctx_msg . data (), ctx_msg . capacity (),
              ' ', ctx_msg . capacity (), ctx_msg . data () );
        memset_while_respecting_language_semantics
            ( ( void * ) cause_msg . data (), cause_msg . capacity (),
              ' ', cause_msg . capacity (), cause_msg . data () );
        memset_while_respecting_language_semantics
            ( ( void * ) suggest_msg . data (), suggest_msg . capacity (),
              ' ', suggest_msg . capacity (), suggest_msg . data () );

        prob_msg . erase ();
        ctx_msg . erase ();
        cause_msg . erase ();
        suggest_msg . erase ();
        stack_frames = -1;
    }

    XP & XP :: operator << ( long long int val )
    {
        if ( val < 0 )
        {
            * which += '-';
            val = - val;
        }

        return operator << ( ( unsigned long long int ) val );
    }

    XP & XP :: operator << ( unsigned long long int val )
    {
        ASCII buffer [ sizeof val * 8 + 16 ];

        const ASCII num [] = "0123456789abcdefghijklmnopqrstuvwxyz";
        assert ( sizeof num >= 2 );
        assert ( radix >= 2 && radix <= sizeof num - 1 );

        count_t i = sizeof buffer;
        do
        {
            buffer [ -- i ] = num [ val % radix ];
            val /= radix;
        }
        while ( val != 0 && i != 0 );

        which -> append ( & buffer [ i ], sizeof buffer - i );

        return * this;
    }

    XP & XP :: operator << ( long double val )
    {
        * which += std :: to_string ( val );
        return * this;
    }

    XP & XP :: operator << ( const std :: string & val )
    {
        * which += val;
        return * this;
    }

    XP & XP :: operator << ( const XMsg & val )
    {
        size_t bytes = val . msg_size;
        if ( bytes < sizeof val . zmsg )
        {
            if ( val . zmsg [ bytes ] == 0 )
                * which += val . zmsg;
            else
            {
                for ( size_t i = 0; i < bytes; ++ i )
                {
                    if ( val . zmsg [ i ] == 0 )
                    {
                        bytes = i;
                        break;
                    }
                }
                which -> append ( val . zmsg, bytes );
            }
        }
        else
        {
            * which += "<BAD MESSAGE BLOCK>";
        }

        return * this;
    }

    void XP :: putChar ( UTF32 ch )
    {
        if ( ch < 128 )
            * which += ( char ) ch;
        else
        {
            UTF8 buffer [ 8 ];
            utf8proc_ssize_t sz = utf8proc_encode_char
                ( ( utf8proc_int32_t ) ch, ( utf8proc_uint8_t * ) buffer );
            if ( sz < 0 )
                * which += "<BAD UTF-32 CHARACTER>";
            else
            {
                which -> append ( buffer, sz );
            }
        }
    }

    void XP :: putUTF8 ( const UTF8 * utf8 )
    {
        if ( utf8 == nullptr )
            utf8 = "<NULL PTR>";

        which -> append ( utf8 );
    }

    void XP :: putUTF8 ( const UTF8 * utf8, size_t bytes )
    {
        if ( utf8 == nullptr )
            * which += "<NULL PTR>";
        else
            which -> append ( utf8, bytes );
    }

    void XP :: putPtr ( const void * ptr )
    {
        * which += "0x";

        unsigned int save = radix;
        radix = 16;

        operator << ( ( unsigned long long int ) ( size_t ) ptr );

        radix = save;
    }

    void XP :: setRadix ( unsigned int r )
    {
        if ( r >= 2 && r <= 36 )
            radix = r;
    }

    void XP :: setStatus ( ReturnCodes status )
    {
        rc = status;
    }

    void XP :: sysError ( int status )
    {
        * which += :: strerror ( status );
    }

    void XP :: cryptError ( int status )
    {
        // using mbedtls
        char buffer [ 512 ];
        mbedtls_strerror ( status, buffer, sizeof buffer );
        * which += buffer;
    }

    void XP :: useProblem ()
    {
        which = & problem;
        radix = 10;
    }

    void XP :: useContext ()
    {
        which = & context;
        radix = 10;
    }

    void XP :: useCause ()
    {
        which = & cause;
        radix = 10;
    }

    void XP :: useSuggestion ()
    {
        which = & suggestion;
        radix = 10;
    }

    XP :: XP ( const UTF8 * file, const ASCII * func, unsigned int line, ReturnCodes status )
        : which ( & problem )
        , file_name ( file )
        , func_name ( func )
        , lineno ( line )
        , radix ( 10 )
        , stack_frames ( -1 )
        , rc ( status )
    {
        // capture stack
        stack_frames = backtrace ( callstack, sizeof callstack / sizeof callstack [ 0 ] );

        // eliminate random path prefix
        const UTF8 * cmn = __FILE__;
        const UTF8 * end = strrchr ( cmn, '/' );
        if ( end != nullptr )
        {
            assert ( strcmp ( end + 1, "except.cpp" ) == 0 );
            end = string_rchr ( cmn, '/', end );
            if ( end != nullptr )
            {
                assert ( strcmp ( end + 1, "secure/except.cpp" ) == 0 );
                size_t cmn_sz = end - cmn + 1;
                if ( memcmp ( file_name, cmn, cmn_sz ) == 0 )
                    file_name += cmn_sz;
            }
        }
    }

    XP :: ~ XP ()
    {
    }

    std :: string format ( const UTF8 * fmt, ... )
    {
        UTF8 buffer [ 1024 ];

        va_list args;
        va_start ( args, fmt );

        int status = vsnprintf ( buffer, sizeof buffer, fmt, args );

        va_end ( args );

        if ( status < 0 )
            status = strlen ( strcpy ( buffer, "<BAD format() PARAMETERS>" ) );

        else if ( ( size_t ) status >= sizeof buffer )
        {
            strcpy ( & buffer [ sizeof buffer - 4 ], "..." );
            status = sizeof buffer - 1;
        }

        return std :: string ( buffer, status );
    }

    bool XBackTrace :: isValid () const noexcept
    {
        return cur_frame < num_frames;
    }

    const XMsg XBackTrace :: getName () const noexcept
    {
        if ( cur_frame >= num_frames )
            return copyXMsg ( "<NO MORE FRAMES>" ); 

        XMsg m;
        const char * name = frames [ cur_frame ];

        size_t i, j, k;
        for ( i = 0; name [ i ] != 0; ++ i )
        {
            // keep frame number
            if ( ! isdigit ( name [ i ] ) )
                break;

            m . zmsg [ i ] = name [ i ];
        }
        for ( ; name [ i ] != 0; ++ i )
        {
            // keep spaces before process name
            if ( ! isspace ( name [ i ] ) )
                break;

            m . zmsg [ i ] = name [ i ];
        }
        for ( j = i; name [ i ] != 0; ++ i )
        {
            // discard process name
            if ( isspace ( name [ i ] ) )
                break;
        }
        for ( ; name [ i ] != 0; ++ i )
        {
            // discard space up to function entrypoint
            if ( ! isspace ( name [ i ] ) )
                break;
        }
        for ( ; name [ i ] != 0; ++ i )
        {
            // discard function entrypoint
            if ( isspace ( name [ i ] ) )
                break;
        }
        for ( k = ++ i; name [ i ] != 0; ++ i )
        {
            // locate the end of the mangled name
            if ( isspace ( name [ i ] ) )
                break;
        }

        // see if the name is mangled
        if ( name [ k ] == '_' && name [ k + 1 ] == 'Z' )
        {
            int status;
            char save = name [ i ];
            ( ( char * ) name ) [ i ] = 0;
            char * real_name = abi :: __cxa_demangle ( & name [ k ], 0, 0, & status );
            ( ( char * ) name ) [ i ] = save;


            for ( k = 0; j < sizeof m . zmsg - 1 && real_name [ k ] != 0; ++ j, ++ k )
                m . zmsg [ j ] = real_name [ k ];

            free ( real_name );

            for ( ; j < sizeof m . zmsg - 1 && name [ i ] != 0; ++ j, ++ i )
                m . zmsg [ j ] = name [ i ];

            m . zmsg [ m . msg_size = j ] = 0;

            return m;
        }

        for ( ; j < sizeof m . zmsg - 1 && name [ k ] != 0; ++ j, ++ k )
            m . zmsg [ j ] = name [ k ];

        m . zmsg [ m . msg_size = j ] = 0;

        return m;
    }

    bool XBackTrace :: up () const noexcept
    {
        if ( cur_frame + 1 < num_frames )
        {
            ++ cur_frame;
            return true;
        }

        return false;
    }

    void XBackTrace :: operator = ( const XBackTrace & bt ) noexcept
    {
        if ( frames != nullptr )
            free ( ( void * ) frames );

        frames = bt . frames;
        num_frames = bt . num_frames;
        cur_frame = bt . cur_frame;

        bt . frames = nullptr;
        bt . num_frames = -1;
        bt . cur_frame = 0;
    }

    XBackTrace :: XBackTrace ( const XBackTrace & bt ) noexcept
        : frames ( bt . frames )
        , num_frames ( bt . num_frames )
        , cur_frame ( bt . cur_frame )
    {
        bt . frames = nullptr;
        bt . num_frames = -1;
        bt . cur_frame = 0;
    }

    void XBackTrace :: operator = ( const XBackTrace && bt ) noexcept
    {
        if ( frames != nullptr )
            free ( ( void * ) frames );

        frames = bt . frames;
        num_frames = bt . num_frames;
        cur_frame = bt . cur_frame;

        bt . frames = nullptr;
        bt . num_frames = -1;
        bt . cur_frame = 0;
    }

    XBackTrace :: XBackTrace ( const XBackTrace && bt ) noexcept
        : frames ( bt . frames )
        , num_frames ( bt . num_frames )
        , cur_frame ( bt . cur_frame )
    {
        bt . frames = nullptr;
        bt . num_frames = -1;
        bt . cur_frame = 0;
    }

    XBackTrace :: XBackTrace ( const Exception & x ) noexcept
        : frames ( nullptr )
        , num_frames ( -1 )
        , cur_frame ( 0 )
    {
        if ( x . stack_frames > 0 )
        {
            frames = backtrace_symbols ( x . callstack, x . stack_frames );
            if ( frames != nullptr )
                num_frames = x . stack_frames;
        }
    }

    XBackTrace :: ~ XBackTrace () noexcept
    {
        if ( frames != nullptr )
        {
            free ( ( void * ) frames );
            frames = nullptr;
        }
        num_frames = -1;
        cur_frame = 0;
    }

    std :: ostream & operator << ( std :: ostream & o, const XBackTrace & bt )
    {
        if ( bt . isValid () )
        {
            do
            {
                o
                    << bt . getName ()
                    << '\n'
                    ;
            }
            while ( bt . up () );
        }

        return o;
    }

}
