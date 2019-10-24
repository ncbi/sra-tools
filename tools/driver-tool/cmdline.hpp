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

#include <ncbi/secure/except.hpp>
#include <ncbi/secure/string.hpp>

#include <vector>
#include <map>

namespace ncbi
{
    /* Cmdline
     *  a structure for holding information related to parsing cmdline
     */
    struct Cmdline
    {
        // add parameters
        template < class T >
        void addParam ( T & value, const String & name, const String & help );

        // indicate start of optional parameters
        void startOptionalParams ();

        // repeating parameter - MUST be final parameter
        template < class T >
        void addParam ( std :: vector < T > & value, U32 min, U32 max, const String & name, const String & help );

        // add boolean options
        void addOption ( bool & value, const String & short_name, const String & long_name, const String & help );
        void addOption ( U32 & value, const String & short_name, const String & long_name, const String & help );

        // add simple options as named parameters
        template < class T >
        void addOption ( T & value, U32 * count, const String & short_name,
            const String & long_name, const String & param_name, const String & help );

        // add list option with separator character
        template < class T >
        void addListOption ( std :: vector < T > & list, const char separator,
            U32 max, const String & short_name, const String & long_name,
            const String & elem_name, const String & help );

        // add trailing cmd - following a '--' will gather but not parse parameters
        void addTrailingCmd ( std :: vector < String > & argv,
            const String & cmd_name, const String & help );

        // add a mode
        void addMode ( const String & mode, const String & help );

        // set current mode
        void setCurrentMode ( const String & mode );

        // get the mode
        U32 getModeInfo ( String & mode ) const;

        void parse ( bool pre_parse = false );
        String nextArg ();
        void help ( bool all_modes = false );
        void shortHelp ( const char * short_name );
        void longHelp ( const char * short_name );

        Cmdline ( int argc, char * argv [] );
        Cmdline ( int argc, char * argv [], const String & vers );
        ~ Cmdline ();

        struct Param
        {
            virtual void handleParam ( const String & text ) const = 0;

            Param ( const String & name, const String & help );
            virtual ~ Param ();

            String name;
            String help;
        };

        struct Option
        {
            virtual void handleOption ( Cmdline & args ) const = 0;

            Option ( const String & short_name, const String & long_name,
                const String & help, bool pre_parse );
            virtual ~ Option ();

            void addParamName ( const String & param_name, char separator = 0 );

            String short_name;
            String long_name;
            std :: vector < String > param_names;
            String help;
            char separator;
            bool pre_parse;
        };

        struct Command
        {
            void addArg ( const String & arg );

            Command ( std :: vector < String > & cmd,
                const String & name, const String & help );
            ~ Command ();

            std :: vector < String > & cmd;
            String name;
            String help;
        };

        struct Mode
        {
            Mode ();
            Mode ( const String & name, const String & help, U32 idx );
            ~ Mode ();

            String mode_name;
            String mode_help;
            std :: vector < Param * > formal_params;
            std :: vector < Option * > formal_opts;
            std :: map < String, Option * > short_opt_map;
            std :: map < String, Option * > long_opt_map;
            Command * trailing_command;

            U32 required_params;
            U32 min_last_param;
            U32 max_last_param;
            U32 mode_idx;
        };

        // for custom parser elements
        void addParam ( Param * param );
        void addOption ( Option * opt );

        Mode * mode;
        std :: map < String, Mode * > modes;
        std :: vector < Mode * > mode_list;
        String vers;

        const char ** argv;
        const char * arg;
        U32 argx;
        U32 argc;

        U32 num_params;
    };

    /*=====================================================*
     *                     EXCEPTIONS                      *
     *=====================================================*/


}
