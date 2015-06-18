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

#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/Reference.hpp>
#include <ngs/Alignment.hpp>
#include <ngs/Pileup.hpp>
#include <ngs/PileupEvent.hpp>

#include <kapp/main.h>
#include <iomanip>

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "ref-variation.vers.h"

namespace RefVariation
{
/*    static void run (  )
    {
        try
        {
            std::cout << "Under construction..." << std::endl;
        }
        catch ( ngs::ErrorMsg & x )
        {
            throw;
        }
        catch ( const char x [] )
        {
            throw;
        }
        catch ( ... )
        {
            throw;
        }
    }*/

    void run ()
    {
    }
}

extern "C"
{
    ver_t CC KAppVersion ()
    {
        return REF_VARIATION_VERS;
    }

    rc_t CC UsageSummary (const char * progname)
    {
        printf (
        "Usage:\n"
        "  %s [options] <accession> [<other accessions, separated by space>]\n"
        "\n"
        "Summary:\n"
        "  Find a possible indel window\n"
        "\n", progname);
        return 0;
    }

    rc_t CC Usage ( struct Args const * args )
    {
        return 0;
    }

    static 
    const char * getArg ( int & i, int argc, char * argv [] )
    {
        
        if ( ++ i == argc )
            throw "Missing argument";
        
        return argv [ i ];
    }

    static
    const char * findArg ( const char*  &arg, int & i, int argc, char * argv [] )
    {
        if ( arg [ 1 ] != 0 )
        {
            const char * next = arg + 1;
            arg = "\0";
            return next;
        }

        return getArg ( i, argc, argv );
    }

    static void handle_help ( const char *appName )
    {
        const char *appLeaf = strrchr ( appName, '/' );
        if ( appLeaf ++ == 0 )
            appLeaf = appName;

        ver_t vers = KAppVersion ();

        /* TODO: add actual help info */
        std :: cout
            << '\n'
            << "Usage:\n"
            << "  " << appLeaf << " [options] <accession>"
            << "\n\n"
            << "Options:\n"
            << "  -o|--output-file                 file for output\n"
            << "                                   (default pipe to stdout)\n"
            << "  -r|--remote-db                   name of remote database to create\n"
            << "                                   (default <accession>.pileup_stat)\n"
            << "  -a|--align-category              the types of alignments to pile up:\n"
            << "                                   { primary, secondary, all } (default all)\n"
            << "  -U|--unpack-integer              don't pack integers in output pipe - uses more bandwidth\n"
            << "  -h|--help                        output brief explanation of the program\n"
            << "  -v|--verbose                     increase the verbosity of the program.\n"
            << "                                   use multiple times for more verbosity.\n"
            << '\n'
            << appName << " : "
            << ( vers >> 24 )
            << '.'
            << ( ( vers >> 16 ) & 0xFF )
            << '.'
            << ( vers & 0xFFFF )
            << '\n'
            << '\n'
            ;
    }

    static void CC handle_error ( const char *arg, void *message )
    {
        throw ( const char * ) message;
    }

    rc_t CC KMain ( int argc, char *argv [] )
    {
        rc_t rc = -1;
        try
        {
            size_t num_runs = 0;
            /* parse command line arguments */
            if ( argc < 2 )
                UsageSummary ( argv[0] );
            for ( int i = 1; i < argc; ++ i )
            {
                const char * arg = argv [ i ];
                if ( arg [ 0 ] != '-' )
                {
                    // have an input run
                    argv [ ++ num_runs ] = ( char* ) arg;
                }
                else do switch ( ( ++ arg ) [ 0 ] )
                {
#if 0
                case 'o':
                    outfile = findArg ( arg, i, argc, argv );
                    break;
                case 'r':
                    remote_db = findArg ( arg, i, argc, argv );
                    break;
                case 'x':
                    ncbi :: depth_cutoff = AsciiToU32 ( findArg ( arg, i, argc, argv ), 
                        handle_error, ( void * ) "Invalid depth cutoff" );
                    break;
                case 'e':
                    ncbi :: event_cutoff = AsciiToU32 ( findArg ( arg, i, argc, argv ), 
                        handle_error, ( void * ) "Invalid event cutoff" );
                    break;
                case 'p':
                    ncbi :: num_significant_bits = AsciiToU32 ( findArg ( arg, i, argc, argv ), 
                        handle_error, ( void * ) "Invalid num-significant-bits" );
                    break;
                case 'a':
                {
                    const char * atype = findArg ( arg, i, argc, argv );
                    if ( strcmp ( atype, "all" ) == 0 )
                        cat = Alignment :: all;
                    else if ( strcmp ( atype, "primary" ) == 0 ||
                              strcmp ( atype, "primaryAlignment" ) == 0 )
                        cat = Alignment :: primaryAlignment;
                    else if ( strcmp ( atype, "secondary" ) == 0 ||
                              strcmp ( atype, "secondaryAlignment" ) == 0 )
                        cat = Alignment :: secondaryAlignment;
                    else
                    {
                        throw "Invalid alignment category";
                    }
                    break;
                }
                case 'U':
                    ncbi :: integer_column_flag_bits = 0;
                    break;
                case 'v':
                    ++ ncbi :: verbosity;
                    break;
#endif
                case 'h':
                case '?':
                    handle_help ( argv [ 0 ] );
                    return 0;
                case '-':
                    ++ arg;
#if 0
                    if ( strcmp ( arg, "output-file" ) == 0 )
                    {
                        outfile = getArg ( i, argc, argv );
                    }
                    else if ( strcmp ( arg, "remote-db" ) == 0 )
                    {
                        remote_db = getArg ( i, argc, argv );
                    }
                    else if ( strcmp ( arg, "buffer-size" ) == 0 )
                    {
                        const char * str = getArg ( i, argc, argv );

                        char * end;
                        long new_buffer_size = strtol ( str, & end, 0 );
                        if ( new_buffer_size < 0 || str == ( const char * ) end || end [ 0 ] != 0 )
                            throw "Invalid buffer argument";

                        buffer_size = new_buffer_size;
                    }
                    else if ( strcmp ( arg, "depth-cutoff" ) == 0 )
                    {
                        ncbi :: depth_cutoff = AsciiToU32 ( getArg ( i, argc, argv ), 
                            handle_error, ( void * ) "Invalid depth cutoff" );
                    }
                    else if ( strcmp ( arg, "event-cutoff" ) == 0 )
                    {
                        ncbi :: event_cutoff = AsciiToU32 ( getArg ( i, argc, argv ),
                            handle_error, ( void * ) "Invalid event cutoff" );
                    }
                    else if ( strcmp ( arg, "num-significant-bits" ) == 0 )
                    {
                        ncbi :: num_significant_bits = AsciiToU32 ( getArg ( i, argc, argv ),
                            handle_error, ( void * ) "Invalid num-significant-bits" );
                    }
                    else if ( strcmp ( arg, "align-category" ) == 0 )
                    {
                        const char * atype = getArg ( i, argc, argv );
                        if ( strcmp ( atype, "all" ) == 0 )
                            cat = Alignment :: all;
                        else if ( strcmp ( atype, "primary" ) == 0 ||
                                  strcmp ( atype, "primaryAlignment" ) == 0 )
                            cat = Alignment :: primaryAlignment;
                        else if ( strcmp ( atype, "secondary" ) == 0 ||
                                  strcmp ( atype, "secondaryAlignment" ) == 0 )
                            cat = Alignment :: secondaryAlignment;
                        else
                        {
                            throw "Invalid alignment category";
                        }
                    }
                    else if ( strcmp ( arg, "unpack-integer" ) == 0 )
                    {
                        ncbi :: integer_column_flag_bits = 0;
                    }
                    else if ( strcmp ( arg, "verbose" ) == 0 )
                    {
                        ++ ncbi :: verbosity;
                    }
                    else
#endif
                    if ( strcmp ( arg, "help" ) == 0 )
                    {
                        handle_help ( argv [ 0 ] );
                        return 0;
                    }
                    else
                    {
                        throw "Invalid Argument";
                    }

                    arg = "\0";

                    break;

                default:
                    throw "Invalid argument";
                }
                while ( arg [ 1 ] != 0 );
            }

#if 0
            if ( num_runs > 1 )
                throw "only one run may be processed at a time";

            for ( int i = 1; i <= num_runs; ++ i )
            {
                ncbi :: run ( argv [ i ], outfile, remote_db, buffer_size, cat );
            }
#endif
            RefVariation::run();

            rc = 0;
        }
        catch ( ngs::ErrorMsg const& x )
        {
            std :: cerr
                << "ERROR: "
                << argv [ 0 ]
                << ": "
                << x . what ()
                << '\n'
                ;
        }
        catch ( const char x [] )
        {
            std :: cerr
                << "ERROR: "
                << argv [ 0 ]
                << ": "
                << x
                << '\n'
                ;
        }
        catch ( ... )
        {
            std :: cerr
                << "ERROR: "
                << argv [ 0 ]
                << ": unknown\n"
                ;
        }

        return rc;
    }
}
