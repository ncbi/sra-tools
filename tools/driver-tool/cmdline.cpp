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

#define __STDC_LIMIT_MACROS 1
#include <stdint.h>

#include <ncbi/secure/except.hpp>

#include "cmdline.hpp"
#include "env.hpp"

#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#if MAC
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif

#define TRACE( lvl, ... ) /* ignore */
int dbg_trace_level = 0;

namespace ncbi
{
    template < class T >
    void convertString ( T & value, const String & text );

    static
    bool prefix_compare ( const char * cstr1, const char * cstr2, bool val )
    {
        for ( size_t i = 0; cstr2 [ i ] != 0; ++ i )
        {
            if ( ( ( const unsigned char * ) cstr1 ) [ i ] !=
                 :: tolower ( ( ( const unsigned char * ) cstr2 ) [ i ] ) )
            {
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "expected boolean value but found '"
                    << cstr2
                    << "'"
                    );
            }
        }
        return val;
    }

    template < >
    void convertString < bool > ( bool & value, const String & text )
    {
        NULTerminatedString ztext ( text );
        const char * cstr2 = ztext . c_str ();
        switch ( :: tolower ( ( ( const unsigned char * ) cstr2 ) [ 0 ] ) )
        {
        case 't':
            value = prefix_compare ( "true", cstr2, true );
            break;
        case 'f':
            value = prefix_compare ( "false", cstr2, false );
            break;
        case 'y':
            value = prefix_compare ( "yes", cstr2, true );
            break;
        case 'n':
            value = prefix_compare ( "no", cstr2, false );
            break;
        case '1':
            value = prefix_compare ( "1", cstr2, true );
            break;
        case '0':
            value = prefix_compare ( "0", cstr2, false );
            break;
        default:
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "expected boolean value but found '"
                << cstr2
                << "'"
                );
        }
    }

    template < >
    void convertString < I32 > ( I32 & value, const String & text )
    {
        char * end;
        NULTerminatedString ztext ( text );
        const char * start = ztext . c_str ();
        I64 i64 = :: strtol ( start, & end, 0 );
        if ( start == ( const char * ) end || end [ 0 ] != 0 )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "expected integer value but found '"
                << start
                << "'"
                );
        }

        if ( i64 > INT32_MAX || i64 < INT32_MIN )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "integer value exceeds 32-bit bounds: "
                << i64
                );
        }

        value = ( I32 ) i64;
    }

    template < >
    void convertString < U32 > ( U32 & value, const String & text )
    {
        char * end;
        NULTerminatedString ztext ( text );
        const char * start = ztext . c_str ();
        U64 u64 = :: strtoul ( start, & end, 0 );
        if ( start == ( const char * ) end || end [ 0 ] != 0 )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "expected natural integer value but found '"
                << start
                << "'"
                );
        }
        
        if ( u64 > UINT32_MAX )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "natural integer value exceeds 32-bit bounds: "
                << u64
                );
        }
        value = ( U32 ) u64;
    }

    template < >
    void convertString < I64 > ( I64 & value, const String & text )
    {
        char * end;
        NULTerminatedString ztext ( text );
        const char * start = ztext . c_str ();
        value = :: strtol ( start, & end, 0 );
        if ( start == ( const char * ) end || end [ 0 ] != 0 )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "expected integer value but found '"
                << start
                << "'"
                );
        }
    }

    template < >
    void convertString < U64 > ( U64 & value, const String & text )
    {
        char * end;
        NULTerminatedString ztext ( text );
        const char * start = ztext . c_str ();
        value = :: strtoul ( start, & end, 0 );
        if ( start == ( const char * ) end || end [ 0 ] != 0 )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "expected natural integer value but found '"
                << start
                << "'"
                );
        }
    }

    template < >
    void convertString < F64 > ( F64 & value, const String & text )
    {
        char * end;
        NULTerminatedString ztext ( text );
        const char * start = ztext . c_str ();
        value = :: strtod ( start, & end );
        if ( start == ( const char * ) end || end [ 0 ] != 0 )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "expected real value but found '"
                << start
                << "'"
                );
        }
#pragma message "TBD - check for NaN, INF, etc."
    }

    template < >
    void convertString < String > ( String & value, const String & text )
    {
        value = text;
        //value . assign ( text );
    }

    Cmdline :: Param :: Param ( const String & _name, const String & _help )
        : name ( _name )
        , help ( _help )
    {
    }

    Cmdline :: Param :: ~ Param ()
    {
    }

    template < class T >
    struct TypedParam : Cmdline :: Param
    {
        virtual void handleParam ( const String & text ) const
        {
            convertString ( value, text );
        }

        TypedParam ( T & _val, const String & name, const String & help )
            : Cmdline :: Param ( name, help )
            , value ( _val )
        {
        }

        T & value;
    };

    template < class T >
    struct RepeatingParam : Cmdline :: Param
    {
        virtual void handleParam ( const String & text ) const
        {
            T param;
            convertString ( param, text );
            value . push_back ( param );
        }

        RepeatingParam ( std :: vector < T > & _val, const String & name, const String & help )
            : Cmdline :: Param ( name, help )
            , value ( _val )
        {
        }

        std :: vector < T > & value;
    };

    Cmdline :: Option :: Option ( const String & _short_name,
            const String & _long_name, const String & _help, bool _pre_parse )
        : short_name ( _short_name )
        , long_name ( _long_name )
        , help ( _help )
        , separator ( 0 )
        , pre_parse ( _pre_parse )
    {
    }

    Cmdline :: Option :: ~ Option ()
    {
    }


    void Cmdline :: Option :: addParamName ( const String & param_name, char _separator )
    {
        if ( separator != 0 )
        {
            NULTerminatedString zlong_name ( long_name );
            NULTerminatedString zparam_name ( param_name );
            
            throw LogicException (
                XP ( XLOC, rc_param_err )
                << "option '--"
                << zlong_name . c_str ()
                << "' cannot accept parameter '"
                << zparam_name . c_str ()
                << "': already a list"
                );
        }
        param_names . push_back ( param_name );
        separator = _separator;
    }


    struct HelpOption : Cmdline :: Option
    {
        virtual void handleOption ( Cmdline & args ) const
        {
            args . help ();
            exit ( 0 );
        }

        HelpOption ( const String & _help )
            : Cmdline :: Option ( "h", "help", _help, true )
        {
        }
    };

#if _DEBUGGING
    struct TraceLevelOption : Cmdline :: Option
    {
        virtual void handleOption ( Cmdline & args ) const
        {
            ++ dbg_trace_level;
        }

        TraceLevelOption ( const String & _help )
            : Cmdline :: Option ( "v", "verbose", _help, true )
        {
        }
    };
#endif

    struct BooleanOption : Cmdline :: Option
    {
        virtual void handleOption ( Cmdline & args ) const
        {
            value = set_value;
        }

        BooleanOption ( bool & _value, const String & _short_name, const String & _long_name, const String & _help )
            : Cmdline :: Option ( _short_name, _long_name, _help, false )
            , value ( _value )
            , set_value ( ! _value )
        {
        }

        bool & value;
        bool set_value;
    };

    struct CountingOption : Cmdline :: Option
    {
        virtual void handleOption ( Cmdline & args ) const
        {
            ++ value;
        }

        CountingOption ( U32 & _value, const String & _short_name, const String & _long_name, const String & _help )
            : Cmdline :: Option ( _short_name, _long_name, _help, false )
            , value ( _value )
        {
        }

        U32 & value;
    };

    template < class T >
    struct TypedOption : Cmdline :: Option
    {
        virtual void handleOption ( Cmdline & args ) const
        {
            String text = args . nextArg ();
            convertString ( value, text );
            if ( count != 0 )
                ++ * count;
        }

        TypedOption ( T & _value, U32 * _count, const String & _short_name,
                const String & _long_name, const String & _param_name, const String & _help )
            : Cmdline :: Option ( _short_name, _long_name, _help, false )
            , value ( _value )
            , count ( _count )
        {
            addParamName ( _param_name );
        }

        T & value;
        U32 * count;
    };

    template < class T >
    struct ListOption : Cmdline :: Option
    {
        void handleElement ( const String & text ) const
        {
            U32 count = ( U32 ) list . size ();
            if ( count >= max )
            {
                NULTerminatedString zlong_name ( long_name );
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "count for list option '--"
                    << zlong_name . c_str ()
                    << "' exceeds maximum of "
                    << max
                    );
            }

            T elem;
            convertString ( elem, text );
            list . push_back ( elem );
        }

        virtual void handleOption ( Cmdline & args ) const
        {
            String elem;
            String text = args . nextArg ();
            size_t start = 0;
            while ( 1 )
            {
                size_t sep = text . find ( separator, start );
                if ( sep == String :: npos )
                    break;

                if ( sep != 0 )
                {
                    elem = text . subString ( start, sep - start );
                    handleElement ( elem );
                }

                start = sep + 1;
            }

            elem = text . subString ( start );
            handleElement ( elem );
        }

        ListOption ( std :: vector < T > & _list, const char separator, U32 _max,
                const String & _short_name, const String & _long_name,
                const String & _elem_name, const String & _help )
            : Cmdline :: Option ( _short_name, _long_name, _help, false )
            , list ( _list )
            , max ( _max )
        {
            addParamName ( _elem_name, separator );
        }

        std :: vector < T > & list;
        U32 max;
    };

    void Cmdline :: Command :: addArg ( const String & arg )
    {
        cmd . push_back ( arg );
    }

    Cmdline :: Command :: Command ( std :: vector < String > & _cmd, const String & _name, const String & _help )
        : cmd ( _cmd )
        , name ( _name )
        , help ( _help )
    {
    }

    Cmdline :: Command :: ~ Command ()
    {
    }

    Cmdline :: Mode :: Mode ()
        : trailing_command ( 0 )
        , required_params ( ( U32 ) -1 )
        , min_last_param ( 0 )
        , max_last_param ( 0 )
        , mode_idx ( 0 )
    {
    }

    Cmdline :: Mode :: Mode ( const String & name, const String & help, U32 idx )
        : mode_name ( name )
        , mode_help ( help )
        , trailing_command ( 0 )
        , required_params ( ( U32 ) -1 )
        , min_last_param ( 0 )
        , max_last_param ( 0 )
        , mode_idx ( idx )
    {
    }

    Cmdline :: Mode :: ~ Mode ()
    {
        size_t i, count= formal_params . size ();
        for ( i = 0; i < count; ++ i )
        {
            Param * param = formal_params [ i ];
            assert ( param != 0 );
            delete param;
        }

        count = formal_opts . size ();
        for ( i = 0; i < count; ++ i )
        {
            Option * opt = formal_opts [ i ];
            assert ( opt != 0 );
            delete opt;
        }

        delete trailing_command;
        trailing_command = 0;

        formal_params . clear ();
        formal_opts . clear ();
        required_params = 0;
        min_last_param = 0;
        max_last_param = 0;
        mode_idx = 0;
    }


    template < class T >
    void Cmdline :: addParam ( T & value, const String & name, const String & help )
    {
        Param * param = new TypedParam < T > ( value, name, help );
        try
        {
            addParam ( param );
        }
        catch ( ... )
        {
            delete param;
            throw;
        }
    }

    template void Cmdline :: addParam < bool > ( bool &, const String &, const String & );
    template void Cmdline :: addParam < I32 > ( I32 &, const String &, const String & );
    template void Cmdline :: addParam < U32 > ( U32 &, const String &, const String & );
    template void Cmdline :: addParam < I64 > ( I64 &, const String &, const String & );
    template void Cmdline :: addParam < U64 > ( U64 &, const String &, const String & );
    template void Cmdline :: addParam < F64 > ( F64 &, const String &, const String & );
    template void Cmdline :: addParam < String > ( String &, const String &, const String & );

    void Cmdline :: startOptionalParams ()
    {
        if ( ( size_t ) mode -> required_params <= mode -> formal_params . size () )
        {
            throw LogicException (
                XP ( XLOC, rc_param_err )
                << "optional param index already set to "
                << mode -> required_params );
        }

        if ( mode -> max_last_param != 0 )
        {
            throw LogicException (
                XP ( XLOC, rc_param_err )
                << "last repeating params have already been set"
                );
        }

        mode -> required_params = ( U32 ) mode -> formal_params . size ();
    }

    template < class T >
    void Cmdline :: addParam ( std :: vector < T > & value, U32 min, U32 max, const String & name, const String & help )
    {
        // there must be a max repeat count, even if it is max(U32)
        if ( max == 0 )
            throw LogicException ( XP ( XLOC, rc_param_err ) << "invalid max count for repeating parameter: 0" );

        // min must be <= max
        if ( min > max )
        {
            throw LogicException (
                XP ( XLOC, rc_param_err )
                << "invalid min count for repeating parameter: "
                << min
                << ">"
                << max
                );
        }

        if ( mode -> max_last_param != 0 )
            throw LogicException ( XP ( XLOC, rc_param_err ) << "last parameter has already been set" );

        // store the repeating positional parameter capture object
        Param * param = new RepeatingParam < T > ( value, name, help );
        try
        {
            addParam ( param );
        }
        catch ( ... )
        {
            delete param;
            throw;
        }

        // update min and max
        mode -> min_last_param = min;
        mode -> max_last_param = max;

        // we now know the number of required parameters
        if ( ( size_t ) mode -> required_params > mode -> formal_params . size () )
            mode -> required_params = ( U32 ) mode -> formal_params . size () + min - 1;
    }

    template void Cmdline :: addParam < bool > ( std :: vector < bool > &, U32, U32, const String &, const String & );
    template void Cmdline :: addParam < I32 > ( std :: vector < I32 > &, U32, U32, const String &, const String & );
    template void Cmdline :: addParam < U32 > ( std :: vector < U32 > &, U32, U32, const String &, const String & );
    template void Cmdline :: addParam < I64 > ( std :: vector < I64 > &, U32, U32, const String &, const String & );
    template void Cmdline :: addParam < U64 > ( std :: vector < U64 > &, U32, U32, const String &, const String & );
    template void Cmdline :: addParam < F64 > ( std :: vector < F64 > &, U32, U32, const String &, const String & );
    template void Cmdline :: addParam < String > ( std :: vector < String > &, U32, U32, const String &, const String & );

    void Cmdline :: addOption ( bool & value, const String & short_name, const String & long_name, const String & help )
    {
        Option * opt = new BooleanOption ( value, short_name, long_name, help );
        try
        {
            addOption ( opt );
        }
        catch ( ... )
        {
            delete opt;
            throw;
        }
    }

    void Cmdline :: addOption ( U32 & value, const String & short_name, const String & long_name, const String & help )
    {
        Option * opt = new CountingOption ( value, short_name, long_name, help );
        try
        {
            addOption ( opt );
        }
        catch ( ... )
        {
            delete opt;
            throw;
        }
    }

    template < class T >
    void Cmdline :: addOption ( T & value, U32 * count, const String & short_name,
        const String & long_name, const String & param_name, const String & help )
    {
        Option * opt = new TypedOption < T > ( value, count, short_name, long_name, param_name, help );
        try
        {
            addOption ( opt );
        }
        catch ( ... )
        {
            delete opt;
            throw;
        }
    }

    template void Cmdline :: addOption < bool > ( bool &, U32 *, const String &, const String &,
        const String &, const String & );
    template void Cmdline :: addOption < I32 > ( I32 &, U32 *, const String &, const String &,
        const String &, const String & );
    template void Cmdline :: addOption < U32 > ( U32 &, U32 *, const String &, const String &,
        const String &, const String & );
    template void Cmdline :: addOption < I64 > ( I64 &, U32 *, const String &, const String &,
        const String &, const String & );
    template void Cmdline :: addOption < U64 > ( U64 &, U32 *, const String &, const String &,
        const String &, const String & );
    template void Cmdline :: addOption < F64 > ( F64 &, U32 *, const String &, const String &,
        const String &, const String & );
    template void Cmdline :: addOption < String > ( String &, U32 *, const String &, const String &,
        const String &, const String & );

    // add list option with separator character
    template < class T >
    void Cmdline :: addListOption ( std :: vector < T > & list, const char separator,
            U32 max, const String & short_name, const String & long_name,
            const String & elem_name, const String & help )
    {
        Option * opt = new ListOption < T > ( list, separator, max, short_name, long_name, elem_name, help );
        try
        {
            addOption ( opt );
        }
        catch ( ... )
        {
            delete opt;
            throw;
        }
    }

    template void Cmdline :: addListOption < bool > ( std :: vector < bool > &, const char, U32,
        const String &, const String &, const String &, const String & );
    template void Cmdline :: addListOption < I32 > ( std :: vector < I32 > &, const char, U32,
        const String &, const String &, const String &, const String & );
    template void Cmdline :: addListOption < U32 > ( std :: vector < U32 > &, const char, U32,
        const String &, const String &, const String &, const String & );
    template void Cmdline :: addListOption < I64 > ( std :: vector < I64 > &, const char, U32,
        const String &, const String &, const String &, const String & );
    template void Cmdline :: addListOption < U64 > ( std :: vector < U64 > &, const char, U32,
        const String &, const String &, const String &, const String & );
    template void Cmdline :: addListOption < F64 > ( std :: vector < F64 > &, const char, U32,
        const String &, const String &, const String &, const String & );
    template void Cmdline :: addListOption < String > ( std :: vector < String > &, const char, U32,
        const String &, const String &, const String &, const String & );


    // add trailing cmd - following a '--' will gather but not parse parameters
    void Cmdline :: addTrailingCmd ( std :: vector < String > & args,
        const String & cmd_name, const String & help )
    {
        NULTerminatedString zcmd_name ( cmd_name );
        if ( zcmd_name . isEmpty () )
            throw LogicException ( XP ( XLOC, rc_param_err ) << "empty command name" );

        if ( mode -> trailing_command != 0 )
            throw LogicException ( XP ( XLOC, rc_param_err ) << "attempt to add multiple trailing commands" );

        mode -> trailing_command = new Command ( args, cmd_name, help );
    }

    void Cmdline :: addMode ( const String & name, const String & help )
    {
        if ( mode_list . size () == 0 )
        {
            std :: pair < String, Mode * > p ( name, mode );
            modes . insert ( p );
            mode_list . push_back ( mode );
            mode -> mode_name = name;
            mode -> mode_help = help;
            mode -> mode_idx = 1;
        }
        else
        {
            Mode * m = new Mode ( name, help, ( U32 ) mode_list . size () + 1 );
            try
            {
                std :: pair < String, Mode * > p ( name, m );
                modes . insert ( p );
                mode_list . push_back ( m );
            }
            catch ( ... )
            {
                delete m;
                throw;
            }
            mode = m;
        }
    }

    void Cmdline :: setCurrentMode ( const String & name )
    {
        std :: map < String, Mode * > :: const_iterator i
            = modes . find ( name );
        if ( i == modes . end () )
        {
            NULTerminatedString zname ( name );
            throw LogicException (
                XP ( XLOC, rc_param_err )
                << "no known mode named '"
                << zname . c_str ()
                << "'"
                );
        }
        mode = i -> second;
    }

    U32 Cmdline :: getModeInfo ( String & name ) const
    {
        name = mode -> mode_name;
        return mode -> mode_idx;
    }

    static
    void add_default_option ( Cmdline & args, Cmdline :: Option * option )
    {
        try
        {
            args . addOption ( option );
        }
        catch ( ... )
        {
            delete option;
        }
    }

    static
    U32 adjust_limits ( Cmdline & args )
    {
        // determine the number of parameters we have
        U32 num_formal_params = ( U32 ) args . mode -> formal_params . size ();

        // fix required param count if all are required
        if ( args . mode -> min_last_param != 0 )
        {
            assert ( num_formal_params != 0 );
            U32 min_formal_params = num_formal_params + args . mode -> min_last_param - 1;
            if ( args . mode -> required_params > min_formal_params )
                args . mode -> required_params = min_formal_params;
        }
        else if ( args . mode -> required_params > num_formal_params )
        {
            args . mode -> required_params = num_formal_params;
        }

        return num_formal_params;
    }

    void Cmdline :: parse ( bool pre_parse )
    {
        num_params = 0;
        argx = 0;

        TRACE ( TRACE_GEEK, "pre_parse = %s\n", pre_parse ? "true" : "false" );
        {
            for ( U32 x = 0; x < argc; ++ x )
            {
                TRACE ( TRACE_GEEK, "argv[%u] = '%s'\n", x, argv[x] );
            }
        }

        // add in default global options
        if ( mode_list . empty () )
        {
            // add in help option if not already there
            TRACE ( TRACE_GEEK, "adding help option\n" );
            add_default_option ( * this, new HelpOption ( "print this message" ) );
#if _DEBUGGING
            TRACE ( TRACE_GEEK, "adding verbose option\n" );
            add_default_option ( * this, new TraceLevelOption ( "increment verbosity" ) );
#endif
        }
        else
        {
            Mode * save = mode;
            size_t i, count = mode_list . size ();
            for ( i = 0; i < count; ++ i )
            {
                mode = mode_list [ i ];
                TRACE ( TRACE_GEEK, "temporarily set mode to '%s'\n", mode -> mode_name . c_str () );
                TRACE ( TRACE_GEEK, "adding help option\n" );
                add_default_option ( * this, new HelpOption ( "print this message" ) );
#if _DEBUGGING
                TRACE ( TRACE_GEEK, "adding verbose option\n" );
                add_default_option ( * this, new TraceLevelOption ( "increment verbosity" ) );
#endif
            }
            mode = save;
            TRACE ( TRACE_GEEK, "restoring mode to '%s'\n", mode -> mode_name . c_str () );
        }

        // look for mode
        if ( modes . size () != 0 )
        {
            TRACE ( TRACE_GEEK, "scanning for mode as first param\n" );
            TRACE ( TRACE_GEEK, "calling nextArg() with argx = %u\n", argx );

            arg = "";
            String mode_name = nextArg ();

            TRACE ( TRACE_GEEK, "mode param is '%s'\n", mode_name . c_str () );
            if ( mode_name . compare ( argv [ 1 ] ) != 0 )
            {
                TRACE ( TRACE_GEEK, "mode param should be '%s'!!! - correcting!!!\n", argv [ 1 ] );
                mode_name = argv [ 1 ];
            }

            TRACE ( TRACE_GEEK, "looking for mode '%s'\n", mode_name . c_str () );
            std :: map < String, Mode * > :: const_iterator i
                = modes . find ( mode_name );
            if ( i == modes . end () )
            {
                if ( mode_name . compare ( "-h" ) == 0 ||
                     mode_name . compare ( "-?" ) == 0 )
                {
                    help ( true );
                    exit ( 0 );
                }

                NULTerminatedString zmode_name ( mode_name );
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "unrecognized keyword: '"
                    << zmode_name . c_str ()
                    << "'"
                    );
            }

            mode = i -> second;
            TRACE ( TRACE_GEEK, "selected mode is '%s'\n", mode -> mode_name . c_str () );
        }

#if _DEBUGGING
        if ( pre_parse )
            dbg_trace_level = 0;
#endif

        // determine the number of parameters we have
        U32 num_formal_params = adjust_limits ( * this );
        U32 max_params = num_formal_params;
        if ( mode -> max_last_param != 0 )
            max_params += mode -> max_last_param - 1;

        // loop over command line
        for ( ++ argx; argx < argc; ++ argx )
        {
            arg = argv [ argx ];
            if ( arg == 0 )
                continue;
            if ( arg [ 0 ] != '-' )
            {
                if ( pre_parse )
                    continue;

                if ( num_params >= max_params )
                {
                    throw InvalidArgument (
                        XP ( XLOC, rc_param_err )
                        << "unexpected parameter["
                        << num_params
                        << "]: '"
                        << arg
                        << "'"
                        );
                }

                // this is taken as a positional parameter
                Param * param;
                if ( num_params < num_formal_params )
                    param = mode -> formal_params [ num_params ];
                else
                {
                    assert ( mode -> max_last_param != 0 );
                    assert ( num_formal_params != 0 );
                    param = mode -> formal_params [ num_formal_params - 1 ];
                }

                String text ( arg );
                arg = "";

                assert ( param != 0 );
                param -> handleParam ( text );
                ++ num_params;
            }
            else if ( arg [ 1 ] == '-' )
            {
                if ( arg [ 2 ] == 0 && mode -> trailing_command != 0 )
                {
                    // this marks a trailing command
                    if ( ! pre_parse )
                    {
                        for ( ++ argx; argx < argc; ++ argx )
                        {
                            arg = argv [ argx ];
                            if ( arg == 0 )
                                continue;
                            mode -> trailing_command -> addArg ( arg );
                        }
                    }

                    // no more command line for anybody else
                    return;
                }
                else
                {
                    // this is a long option
                    std :: map < String, Option * > :: const_iterator i
                        = mode -> long_opt_map . find ( & arg [ 2 ] );
                    if ( i == mode -> long_opt_map . end () )
                    {
                        if ( pre_parse )
                            continue;

                        throw InvalidArgument (
                            XP ( XLOC, rc_param_err )
                            << "unrecognized option: '"
                            << arg
                            << "'"
                            );
                    }

                    // call the option handler
                    Option * opt = i -> second;
                    arg = "";

                    assert ( opt != 0 );
                    if ( opt -> pre_parse == pre_parse )
                        opt -> handleOption ( * this );
                    else
                    {
                        size_t opt_params = opt -> param_names . size ();
                        for ( size_t j = 0; j < opt_params; ++ j )
                        {
                            String dummy = nextArg ();
                        }
                    }
                }
            }
            else
            {
                // skip over dash
                ++ arg;

                // measure all remaining characters
                // these can be one or more options
                // and can be suffixed with parameters to the last option
                U32 len, mlen = strlen ( arg );

                // process each short option in a potentially compound list
                do
                {
                    bool found = false;

                    // find short option, starting with len 1 up to mlen
                    for ( len = 1; len <= mlen; ++ len )
                    {
                        String key ( arg, len );
                        std :: map < String, Option * > :: iterator i
                            = mode -> short_opt_map . find ( key );
                        if ( i != mode -> short_opt_map . end () )
                        {
                            // found the key, so consume the short name
                            arg += len;
                            mlen -= len;
                            found = true;

                            // call the option handler
                            Option * opt = i -> second;
                            assert ( opt != 0 );
                            if ( opt -> pre_parse == pre_parse )
                                opt -> handleOption ( * this );
                            else
                            {
                                size_t opt_params = opt -> param_names . size ();
                                for ( size_t j = 0; j < opt_params; ++ j )
                                {
                                    String dummy = nextArg ();
                                }
                            }
                            break;
                        }
                    }

                    // if short name was not found, blow exception
                    if ( ! found && ! pre_parse )
                    {
                        throw InvalidArgument (
                            XP ( XLOC, rc_param_err )
                            << "unrecognized option: '-"
                            << arg
                            << "'"
                            );
                    }
                }
                while ( arg [ 0 ] != 0 );
            }
        }

        // our only check is for the minimum number of parameters
        if ( ! pre_parse && num_params < mode -> required_params )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "insufficient parameters: expected "
                << mode -> required_params
                << "but observed "
                << num_params
                );
        }
    }

    String Cmdline :: nextArg ()
    {
        const char * val = arg;
        arg = "";

        assert ( val != 0 );
        if ( val [ 0 ] == 0 )
        {
            if ( ++ argx == argc )
                throw InvalidArgument ( XP ( XLOC, rc_param_err ) << "incomplete command line - expected parameter" );

            val = argv [ argx ];
            TRACE ( TRACE_USR, "argx is %u, param is '%s'\n", argx, val );
        }

        return String ( val );
    }

    // play with these guys
    const size_t help_start = 35;
    const size_t right_edge = 78;

    static
    void print_help_text ( const String & help_text )
    {
        size_t help_width = right_edge - help_start;

        String help_str = help_text;
        while ( help_str . size () > help_width )
        {
            size_t sep = help_str . rfind ( ' ', help_width );
            if ( sep == String :: npos )
                break;
            std :: cout
                << help_str . subString ( 0, sep )
                << '\n'
                ;
                        
            help_str = help_str . subString ( sep + 1 );
            help_width = right_edge - help_start - 2;

            std :: cout . fill ( ' ' );
            std :: cout . width ( help_start + 2 );
            std :: cout << ' ';
        }

        std :: cout
            << help_str
            << '\n'
            ;
    }

    void Cmdline :: shortHelp ( const char * short_name )
    {
        std :: cout
            << short_name
            ;

        U32 mode_count = ( U32 ) mode_list . size ();
        if ( mode_count != 0 )
        {
            std :: cout
                << ' '
                << mode -> mode_name
                ;
        }

        U32 formal_opt_count = ( U32 ) mode -> formal_opts . size ();
        if ( formal_opt_count != 0 )
        {
            std :: cout
                << " [ options ]"
                ;
        }

        U32 i, formal_param_count = adjust_limits ( * this );
        for ( i = 0; i < formal_param_count && i < mode -> required_params; ++ i )
        {
            Param * param = mode -> formal_params [ i ];
            std :: cout
                << ' '
                << param -> name
                ;
        }

        U32 num_optional_params = formal_param_count - i;
        if ( num_optional_params == 0 )
            num_optional_params = mode -> max_last_param - mode -> min_last_param;
        if ( num_optional_params != 0 )
        {
            std :: cout
                << " ["
                ;

            bool last_formal_is_required = true;
            for ( ; i < formal_param_count; ++ i )
            {
                Param * param = mode -> formal_params [ i ];
                std :: cout
                    << ' '
                    << param -> name
                    ;
                last_formal_is_required = false;
            }

            if ( mode -> max_last_param > mode -> min_last_param )
            {
                if ( last_formal_is_required )
                {
                    Param * param = mode -> formal_params [ i - 1 ];
                    std :: cout
                        << ' '
                        << param -> name
                        ;
                }
                std :: cout
                    << "..."
                    ;
            }

            std :: cout
                << " ]"
                ;
        }

        if ( mode -> trailing_command != 0 )
        {
            std :: cout
                << " -- "
                << mode -> trailing_command -> name
                ;
        }
    }

    void Cmdline :: longHelp ( const char * short_name )
    {
        U32 i, mode_count = ( U32 ) mode_list . size ();
        U32 formal_opt_count = ( U32 ) mode -> formal_opts . size ();
        U32 formal_param_count = adjust_limits ( * this );

        if ( mode_count != 0 )
        {
            std :: cout
                << '\n'
                << '\n'
                ;

            std :: cout . fill ( ' ' );
            std :: cout . width ( help_start - 6 );

            std :: cout
                << "***"
                << '\n'
                << '\''
                << mode -> mode_name
                << "' mode:\n"
                << '\n'
                << "  "
                << mode -> mode_help
                << '\n'
                << "  $ "
                ;
            shortHelp ( short_name );
        }

        if ( formal_param_count != 0 )
        {
            std :: cout
                << '\n'
                << '\n'
                << "Parameters:\n"
                << '\n'
                ;

            for ( i = 0; i < formal_param_count; ++ i )
            {
                Param * param = mode -> formal_params [ i ];
                std :: cout
                    << "  "
                    << param -> name
                    ;

                size_t chars = 2 + param -> name . size ();
                std :: cout . fill ( ' ' );
                std :: cout . width ( help_start - chars );
                std :: cout << ' ';
                print_help_text ( param -> help );
            }
        }

        if ( formal_opt_count != 0 )
        {
            std :: cout
                << '\n'
                << '\n'
                << "Options:\n"
                << '\n'
                ;

            // find the longest option "short name"
            size_t longest_short_name = 0;
            for ( i = 0; i < formal_opt_count; ++ i )
            {
                Option * opt = mode -> formal_opts [ i ];
                size_t short_len = opt -> short_name . size ();
                if ( short_len > longest_short_name )
                    longest_short_name = short_len;
            }

            // the short-name field width is now constant
            size_t short_name_field_width
                = 1                    // for dash
                + 1                    // for pipe separator
                + longest_short_name   // for name itself
                ;

            // now walk the options
            for ( i = 0; i < formal_opt_count; ++ i )
            {
                size_t chars = 2;
                Option * opt = mode -> formal_opts [ i ];
                if ( opt -> short_name . isEmpty () )
                {
                    std :: cout . fill ( ' ' );
                    std :: cout . width ( short_name_field_width );
                    std :: cout << ' ';
                    
                    std :: cout
                        << "  --"
                        << opt -> long_name
                        ;
                    chars += short_name_field_width + opt -> long_name . size () + 2;
                }
                else if ( opt -> long_name . isEmpty () )
                {
                    std :: cout
                        << "  -"
                        << opt -> short_name
                        ;
                    chars += opt -> short_name . size () + 1;
                }
                else
                {
                    std :: cout
                        << "  -"
                        << opt -> short_name
                        ;
                    
                    if ( opt -> short_name . size () < longest_short_name )
                    {
                        std :: cout . fill ( ' ' );
                        std :: cout . width ( longest_short_name - opt -> short_name . size () );
                        std :: cout << ' ';
                    }
                    
                    std :: cout
                        << "|--"
                        << opt -> long_name
                        ;
                    chars += short_name_field_width + opt -> long_name . size () + 2;
                }
                
                // print option parameters
                size_t opt_params = opt -> param_names . size ();
                for ( size_t j = 0; j < opt_params; ++ j )
                {
                    const String & param_name = opt -> param_names [ j ];
                    chars += param_name . size () + 1;
                    if ( chars >= right_edge )
                    {
                        std :: cout
                            << '\n'
                            << "       "
                            ;
                        chars = param_name . size () + 8;
                    }
                    
                    std :: cout
                        << ' '
                        << param_name
                        ;

                    if ( opt -> separator != 0 && j + 1 == opt_params )
                    {
                        std :: cout
                            << '['
                            << opt -> separator
                            << "...]"
                            ;
                        chars += 6;
                    }
                }

                // if the options ran too far into help text
                if ( chars >= help_start )
                {
                    std :: cout
                        << '\n'
                        ;
                    chars = 0;
                }
                
                // fill to help start - 1
                if ( chars < help_start )
                {
                    std :: cout . fill ( ' ' );
                    std :: cout . width ( help_start - chars );
                    std :: cout << ' ';
                }

                print_help_text ( opt -> help );
            }
        }

        if ( mode -> trailing_command != 0 )
        {
            std :: cout
                << '\n'
                << '\n'
                << "Trailing Command:\n"
                << '\n'
                ;

            std :: cout
                << "  "
                << mode -> trailing_command -> name
                ;

            size_t chars = 2 + mode -> trailing_command -> name . size ();
            std :: cout . fill ( ' ' );
            std :: cout . width ( help_start - chars );
            std :: cout << ' ';
            print_help_text ( mode -> trailing_command -> help );
        }
    }

    void Cmdline :: help ( bool all_modes )
    {
        const char * short_name = strrchr ( argv [ 0 ], '/' );
        if ( short_name ++ == 0 )
            short_name = argv [ 0 ];

        U32 i, mode_count = mode_list . size ();
        if ( mode_count == 0 )
        {
            std :: cout
                << '\n'
                << "Usage: "
                ;

            shortHelp ( short_name );
        }
        else
        {
            Mode * save = mode;
            for ( i = 0; i < mode_count; ++ i )
            {
                mode = mode_list [ i ];

                std :: cout
                    << '\n'
                    << "Usage: "
                    ;

                shortHelp ( short_name );
            }
            mode = save;
        }

        if ( ! all_modes || mode_count == 0 )
            longHelp ( short_name );
        else
        {
            Mode * save = mode;
            for ( i = 0; i < mode_count; ++ i )
            {
                mode = mode_list [ i ];
                longHelp ( short_name );
            }
            mode = save;
        }

        std :: cout
            << '\n'
            << '"'
            << argv [ 0 ]
            << '"'
            ;

        if ( ! vers . isEmpty () )
        {
            std :: cout
                << " version "
                << vers
                ;
        }

        std :: cout
            << '\n'
            << '\n'
            ;
    }

    Cmdline :: Cmdline ( int _argc, char * _argv [] )
        : mode ( new Mode )
        , argv ( ( const char ** ) _argv )
        , arg ( "" )
        , argx ( 1 )
        , argc ( _argc )
        , num_params ( 0 )
    {
        if ( _argc <= 0 )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "illegal argument count - "
                << _argc
                );
        }
        if ( argv == 0 )
            throw InvalidArgument ( XP ( XLOC, rc_param_err ) << "null argument vector" );
    }

    Cmdline :: Cmdline ( int _argc, char * _argv [], const String & _vers )
        : mode ( new Mode )
        , vers ( _vers )
        , argv ( ( const char ** ) _argv )
        , arg ( "" )
        , argx ( 1 )
        , argc ( _argc )
        , num_params ( 0 )
    {
        if ( _argc <= 0 )
        {
            throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "illegal argument count - "
                << _argc
                );
        }
        if ( argv == 0 )
            throw InvalidArgument ( XP ( XLOC, rc_param_err ) << "null argument vector" );
    }

    Cmdline :: ~ Cmdline ()
    {
        size_t i, count = mode_list . size ();
        if ( count == 0 )
        {
            delete mode;
            mode = 0;
        }
        else
        {
            mode = 0;
            for ( i = 0; i < count; ++ i )
            {
                delete mode_list [ i ];
            }

            modes . clear ();
            mode_list . clear ();
        }
    }

    void Cmdline :: addParam ( Param * param )
    {
        if ( param == 0 )
            throw LogicException ( XP ( XLOC, rc_param_err ) <<  "attempt to add null formal parameter" );

        // if any repeating param has been set, it's last
        if ( mode -> max_last_param != 0 )
        {
            NULTerminatedString zname ( param -> name );
            throw LogicException (
                XP ( XLOC, rc_param_err )
                << "cannot add param '"
                << zname . c_str ()
                << "' after repeating parameter"
                );
        }

        // store the repeating positional parameter capture object
        mode -> formal_params . push_back ( param );
    }

    void Cmdline :: addOption ( Option * opt )
    {
        if ( opt == 0 )
            throw LogicException ( XP ( XLOC, rc_param_err ) <<  "attempt to add null formal option" );

        if ( opt -> long_name . isEmpty () && opt -> short_name . isEmpty () )
            throw LogicException ( XP ( XLOC, rc_param_err ) << "attempt to add formal option with empty names" );

        // attempt to insert into long name map
        if ( ! opt -> long_name . isEmpty () )
        {
            std :: pair < String, Option * > long_pair ( opt -> long_name, opt );
            std :: pair < std :: map < String, Option * > :: iterator, bool > long_found
                = mode -> long_opt_map . insert ( long_pair );
            if ( ! long_found . second )
            {
                NULTerminatedString zlong_name ( opt -> long_name );
                throw LogicException (
                    XP ( XLOC, rc_param_err )
                    <<  "formal option '--"
                    << zlong_name . c_str ()
                    << "' already exists"
                    );
            }
        }

        if ( ! opt -> short_name . isEmpty () )
        {
            // attempt to insert into short name map
            std :: pair < String, Option * > short_pair ( opt -> short_name, opt );
            std :: pair < std :: map < String, Option * > :: iterator, bool > short_found
                = mode -> short_opt_map . insert ( short_pair );
            if ( ! short_found . second )
            {
                NULTerminatedString zshort_name ( opt -> short_name );
                throw LogicException (
                    XP ( XLOC, rc_param_err )
                    <<  "formal option '-"
                    << zshort_name . c_str ()
                    << "' already exists"
                    );
            }

            // test to see if short name is confusable with any other by prefix
            const String & oshort = opt -> short_name;
            size_t count = mode -> formal_opts . size ();
            for ( size_t i = 0; i < count; ++ i )
            {
                const Option * prior = mode -> formal_opts [ i ];
                const String & pshort = prior -> short_name;
                if ( pshort . isEmpty () )
                    continue;
                if ( pshort . size () < oshort . size () )
                {
                    if ( memcmp ( pshort . data (), oshort . data (), pshort . size () ) == 0 )
                    {
                        NULTerminatedString zoshort ( oshort );
                        NULTerminatedString zpshort ( pshort );
                        throw LogicException (
                            XP ( XLOC, rc_param_err )
                            << "formal option '-"
                            << zoshort . c_str ()
                            << "' conflicts with '-"
                            << zpshort . c_str ()
                            << "'"
                            );
                    }
                }
                else
                {
                    if ( memcmp ( pshort . data (), oshort . data (), oshort . size () ) == 0 )
                    {
                        NULTerminatedString zoshort ( oshort );
                        NULTerminatedString zpshort ( pshort );
                        throw LogicException (
                            XP ( XLOC, rc_param_err )
                            << "formal option '-"
                            << zoshort . c_str ()
                            << "' conflicts with '-"
                            << zpshort . c_str ()
                            << "'"
                            );
                    }
                }
            }
        }

        // accept it
        mode -> formal_opts . push_back ( opt );
    }

    /* EnvImport
     *  a structure for importing startup parameters
     */

    EnvImport :: Import :: Import ( const String & _name )
        : name ( _name )
    {
    }

    EnvImport :: Import :: ~ Import ()
    {
    }

    template < class T >
    struct TypedImport : EnvImport :: Import
    {
        void handleParam ( const String & text ) const
        {
            convertString ( val, text );
        }

        TypedImport ( T & _val, const String & name )
            : EnvImport :: Import ( name )
            , val ( _val )
        {
        }

        T & val;
    };

    template < class T >
    struct ListImport : EnvImport :: Import
    {
        void handleElement ( const String & text ) const
        {
            U32 count = ( U32 ) list . size ();
            if ( count >= max )
            {
                NULTerminatedString zname ( name );
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "count for list variable '"
                    << zname . c_str ()
                    << "' exceeds maximum of "
                    << max
                    );
            }

            T elem;
            convertString ( elem, text );
            list . push_back ( elem );
        }

        void handleParam ( const String & text ) const
        {
            String elem;
            size_t start = 0;
            while ( 1 )
            {
                size_t sep = text . find ( separator, start );
                if ( sep == String :: npos )
                    break;

                elem = text . subString ( start, sep - start );
                handleElement ( elem );
                start = sep + 1;
            }

            elem = text . subString ( start );
            handleElement ( elem );
        }

        ListImport ( std :: vector < T > & _list, const char _separator, U32 _max, const String & name )
            : EnvImport :: Import ( name )
            , list ( _list )
            , max ( _max )
            , separator ( _separator )
        {
        }

        std :: vector < T > & list;
        U32 max;
        char separator;
    };

    template < class T >
    void EnvImport :: addParam ( T & value, const String & name )
    {
        Import * import = new TypedImport < T > ( value, name );
        try
        {
            addParam ( import );
        }
        catch ( ... )
        {
            delete import;
            throw;
        }
    }

    template void EnvImport :: addParam < bool > ( bool &, const String & );
    template void EnvImport :: addParam < I32 > ( I32 &, const String & );
    template void EnvImport :: addParam < U32 > ( U32 &, const String & );
    template void EnvImport :: addParam < I64 > ( I64 &, const String & );
    template void EnvImport :: addParam < U64 > ( U64 &, const String & );
    template void EnvImport :: addParam < F64 > ( F64 &, const String & );
    template void EnvImport :: addParam < String > ( String &, const String & );

    template < class T >
    void EnvImport :: addList ( std :: vector < T > & list, const char separator, U32 max, const String & name )
    {
        Import * import = new ListImport < T > ( list, separator, max, name );
        try
        {
            addParam ( import );
        }
        catch ( ... )
        {
            delete import;
            throw;
        }
    }

    template void EnvImport :: addList < bool > ( std :: vector < bool > &, const char, U32, const String & );
    template void EnvImport :: addList < I32 > ( std :: vector < I32 > &, const char, U32, const String & );
    template void EnvImport :: addList < U32 > ( std :: vector < U32 > &, const char, U32, const String & );
    template void EnvImport :: addList < I64 > ( std :: vector < I64 > &, const char, U32, const String & );
    template void EnvImport :: addList < U64 > ( std :: vector < U64 > &, const char, U32, const String & );
    template void EnvImport :: addList < F64 > ( std :: vector < F64 > &, const char, U32, const String & );
    template void EnvImport :: addList < String > ( std :: vector < String > &, const char, U32, const String & );

    static
    void add_default_param ( EnvImport & env, EnvImport :: Import * import )
    {
        try
        {
            env . addParam ( import );
        }
        catch ( ... )
        {
            delete import;
        }
    }

    void EnvImport :: import ()
    {
        if ( environ != 0 )
        {
            /*
#if _DEBUGGING
            add_default_param ( * this, new TypedImport < int > ( dbg_trace_fd, "dbg_trace_fd" ) );
            add_default_param ( * this, new TypedImport < int > ( dbg_dedicated_log, "dedicated_log" ) );
            add_default_param ( * this, new TypedImport < unsigned int > ( dbg_trace_level, "dbg_trace_level" ) );
#endif
            */
            const char *pfx = 0;
            size_t psz = prefix . size ();
            if ( psz != 0 )
                pfx = prefix . data ();

            for ( U32 i = 0; environ [ i ] != 0; ++ i )
            {
                const char * nvp = environ [ i ];
                if ( pfx != 0 )
                {
                    if ( memcmp ( nvp, pfx, psz ) != 0 || nvp [ psz ] != '_' )
                        continue;
                    nvp += psz + 1;
                }

                const char * sep = strchr ( nvp, '=' );
                if ( sep != 0 )
                {
                    String name ( nvp, sep - nvp );
                    String value ( sep + 1 );

                    std :: map < String, Import * > :: const_iterator it
                        = name_map . find ( name );
                    if ( it != name_map . end () )
                    {
                        Import * import = it -> second;
                        assert ( import != 0 );
                        import -> handleParam ( value );
                    }
                }
            }
        }
    }

    EnvImport :: EnvImport ()
    {
    }

    EnvImport :: EnvImport ( const String & _prefix )
        : prefix ( _prefix )
    {
    }

    EnvImport :: ~ EnvImport ()
    {
        size_t count = imports . size ();
        for ( size_t i = 0; i < count; ++ i )
        {
            Import * import = imports [ i ];
            delete import;
        }

        imports . clear ();
    }
    
    void EnvImport :: addParam ( Import * import )
    {
        if ( import == 0 )
            throw LogicException ( XP ( XLOC, rc_param_err ) << "attempt to add null env param" );

        if ( import -> name . isEmpty () )
            throw LogicException ( XP ( XLOC, rc_param_err ) << "attempt to add env param with empty name" );

        // attempt to insert into name map
        std :: pair < String, Import * > pair ( import -> name, import );
        std :: pair < std :: map < String, Import * > :: iterator, bool > found
                = name_map . insert ( pair );
        if ( ! found . second )
        {
            NULTerminatedString zname ( import -> name );
            throw LogicException (
                XP ( XLOC, rc_param_err )
                << "env param '"
                << zname . c_str ()
                << "' already exists"
                );
        }

        // accept it
        imports . push_back ( import );
    }
}
