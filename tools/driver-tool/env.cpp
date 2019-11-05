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

#include "env.hpp"
#include "logging.hpp"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if MAC
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif

#define TRACE( lvl, ... ) /* ignore */

namespace ncbi
{
    size_t EnvExport :: count () const
    {
        size_t size = env . size ();
        size_t count = kids . size ();
        for ( size_t i = 0; i < count; ++ i )
        {
            EnvExport * kid = kids [ i ];
            assert ( kid != 0 );
            size += kid -> count ();
        }
        return size;
    }

    void EnvExport :: set ( const String & name, const String & val )
    {
        TRACE ( TRACE_QA, "setting %s environment variable '%s=%s'\n"
                , scope . c_str ()
                , name . c_str ()
                , val . c_str ()
            );

        std :: pair < String, String > pair ( name, val );
        std :: pair < std :: map < String, String > :: iterator, bool > result
            = env . insert ( pair );
        if ( ! result . second )
        {
            // already existed - overwrite
            TRACE ( TRACE_QA, "overwriting %s environment variable '%s' from '%s'\n"
                    , scope . c_str ()
                    , name . c_str ()
                    , result . first -> second . c_str ()
                );
            result . first -> second = val;
        }
    }

    void EnvExport :: add ( const String & name, const String & val, char sep )
    {
        TRACE ( TRACE_QA, "adding %s environment variable '%s=%s'\n"
                , scope . c_str ()
                , name . c_str ()
                , val . c_str ()
            );

        std :: pair < String, String > pair ( name, val );
        std :: pair < std :: map < String, String > :: iterator, bool > result
            = env . insert ( pair );
        if ( ! result . second )
        {
            // already existed - append with separator
            TRACE ( TRACE_QA, "extending %s environment variable '%s' from '%s'\n"
                    , scope . c_str ()
                    , name . c_str ()
                    , result . first -> second . c_str ()
                );

            StringBuffer newVal;
            newVal . append ( result . first -> second ) . append ( sep ) . append ( val );
            result . first -> second = newVal . stealString ();
        }
    }

    void EnvExport :: unset ( const String & name )
    {
        TRACE ( TRACE_QA, "unsetting %s environment variable '%s'\n"
                , scope . c_str ()
                , name . c_str ()
            );
        env . erase ( name );
    }

    void EnvExport :: import ( const String & prefix )
    {
        TRACE ( TRACE_QA, "importing all environment variables with prefix '%s_'\n", prefix . c_str () );

        if ( environ != 0 )
        {
            const char * pfx = prefix . data ();
            size_t psz = prefix . size ();

            for ( uint32_t i = 0; environ [ i ] != 0; ++ i )
            {
                const char * nvp = environ [ i ];
                if ( memcmp ( nvp, pfx, psz ) == 0 && nvp [ psz ] == '_' )
                {
                    nvp += psz + 1;
                    const char * sep = strchr ( nvp, '=' );
                    if ( sep != 0 )
                    {
                        String name ( nvp, sep - nvp );
                        String value ( sep + 1 );
                        add ( name, value );
                    }
                }
            }
        }
    }

    static
    void make_entry ( char * & slot, const String & prefix,
        const String & name, const String & val )
    {
        size_t bytes = prefix . size () + name . size () + val . size () + 2;
        slot = new char [ bytes ];
        snprintf ( slot, bytes, "%s%s=%s",
                   NULTerminatedString ( prefix  ) . c_str (),
                   NULTerminatedString ( name ) . c_str (),
                   NULTerminatedString ( val ) . c_str () );
    }

    static
    void make_prefix ( String & prefix, EnvExport * env )
    {
        if ( env -> dad != 0 )
        {
            make_prefix ( prefix, env -> dad );

            StringBuffer newVal;
            newVal . append ( prefix ) . append ( env -> scope ) . append ( '_' );
            prefix = newVal . stealString ();
        }
    }

    size_t EnvExport :: populate ( char ** envp, size_t size )
    {
        TRACE ( TRACE_QA, "populating envp with %s environment\n", scope . c_str () );
        size_t cnt = count ();
        if ( cnt >= size )
        {
            log . msg ( LOG_ERR )
                << "ERROR: insufficient environment slots"
                << endm
                ;
            throw LogicException ( XP ( XLOC ) << "ERROR: insufficient environment slots" );
        }

        String prefix;
        make_prefix ( prefix, this );

        size_t i;
        std :: map < String, String > :: const_iterator it
            = env . begin ();
        for ( i = 0; it != env . end () && i < cnt; ++ it, ++ i )
            make_entry ( envp [ i ], prefix, it -> first, it -> second );

        if ( it != env . end () )
        {
            log . msg ( LOG_ERR )
                << "ERROR: environment contents changed during processing"
                << endm
                ;
            throw LogicException ( XP ( XLOC ) << "ERROR: environment contents changed during processing" );
        }

        envp [ i ] = 0;

        cnt = populateKids ( envp + i, size - i );

        return cnt + i;
    }

    size_t EnvExport :: populateKids ( char ** envp, size_t size )
    {
        size_t total = 0;;
        size_t count = kids . size ();
        for ( size_t i = 0; i < count; ++ i )
        {
            EnvExport * kid = kids [ i ];
            size_t cnt = kid -> populate ( & envp [ total ], size - total );
            total += cnt;
        }
        return total;
    }

    void EnvExport :: link ( EnvExport * _dad )
    {
        if ( dad != 0 )
        {
            log . msg ( LOG_ERR )
                << "ERROR: sub-environment already linked"
                << endm
                ;
            throw LogicException ( XP ( XLOC ) << "ERROR: sub-environment already linked" );
        }

        dad = _dad;

        TRACE ( TRACE_QA, "linking %s environment under %s\n"
                , scope . c_str ()
                , dad -> scope . c_str ()
            );

        dad -> kids . push_back ( this );
    }

    EnvExport :: EnvExport ( const String & _scope )
        : scope ( _scope )
        , dad ( 0 )
    {
    }

    EnvExport :: ~ EnvExport ()
    {
        dad = 0;
    }
    
}
