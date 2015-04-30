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

#define USE_GENERAL_LOADER 1
#define RECORD_REF_BASE    0
#define RECORD_REF_POS     1
#define RECORD_MATCH_COUNT 1

#if _DEBUGGING
#define SINGLE_REFERENCE 0
#define SLICE_START      0
#define SLICE_WIDTH      0
#define NO_PILEUP_EVENTS 0
#define MIN_REPORT_DEPTH 20000
#endif

#include "../general-loader/general-writer.hpp"

#include "pileup-stats.vers.h"

#include <iostream>
#include <string.h>
#include <ctype.h>

using namespace ngs;

namespace ncbi
{
    enum
    {
        col_RUN_NAME,
        col_REFERENCE_SPEC,
#if RECORD_REF_POS
        col_REF_POS,
#endif
#if RECORD_REF_BASE
        col_REF_BASE,
#endif
        col_DEPTH,
        col_MISMATCH_COUNTS,
        col_INSERTION_COUNTS,
        col_DELETION_COUNT,

        num_columns
    };

#if USE_GENERAL_LOADER
    static int tbl_id;
    static int column_id [ num_columns ];
    static uint8_t integer_column_flag_bits;
#endif

    static uint32_t depth_cutoff = 1;               // do not output if depth <= this value

    static uint32_t verbosity;

    static
    void run (
#if USE_GENERAL_LOADER
        GeneralWriter & out,
#endif
        const String & runName, const String & refName, PileupIterator & pileup )
    {
        for ( int64_t ref_zpos = -1; pileup . nextPileup (); ++ ref_zpos )
        {
            if ( ref_zpos < 0 )
                ref_zpos = pileup . getReferencePosition ();

            switch ( verbosity )
            {
            case 0:
                break;
            case 1:
                if ( ( ref_zpos % 1000000 ) == 0 )
                    std :: cerr << "#  " << std :: setw ( 9 ) << ref_zpos << '\n';
                break;
            default:
                if ( ( ref_zpos % 5000 ) == 0 )
                {
                    if ( ( ref_zpos % 500000 ) == 0 )
                        std :: cerr << "\n#  " << std :: setw ( 9 ) << ref_zpos << ' ';
                    std :: cerr << '.';
                }
            }

            uint32_t ref_base_idx = 0;
            char ref_base = pileup . getReferenceBase ();
            switch ( toupper ( ref_base ) )
            {
            case 'A': break;
            case 'C': ref_base_idx = 1; break;
            case 'G': ref_base_idx = 2; break;
            case 'T': ref_base_idx = 3; break;
            default: continue;
            }

            uint32_t depth = pileup . getPileupDepth ();
#if NO_PILEUP_EVENTS
            ( void ) ref_base_idx;
#if ! USE_GENERAL_LOADER
            if ( depth > MIN_REPORT_DEPTH )
            {
                std :: cout
                    << runName
                    << '\t' << refName
                    << '\t' << ref_zpos + 1
                    << '\t' << ref_base
                    << '\t' << depth
                    << '\n'
                    ;
            }
#endif
#else

#if RECORD_MATCH_COUNT
            uint32_t mismatch_counts [ 4 ];
#else
            uint32_t mismatch_counts [ 3 ];
#endif
            memset ( mismatch_counts, 0, sizeof mismatch_counts );

            uint32_t ins_counts [ 4 ];
            memset ( ins_counts, 0, sizeof ins_counts );

            uint32_t del_cnt = 0;

            bool have_mismatch = false;
            bool have_inserts = false;


            if ( depth > depth_cutoff )
            {
                char mismatch;
                uint32_t mismatch_idx;

                while ( pileup . nextPileupEvent () )
                {
                    PileupEvent :: PileupEventType et = pileup . getEventType ();
                    switch ( et & 7 )
                    {
                    case PileupEvent :: match:
#if RECORD_MATCH_COUNT
                        ++ mismatch_counts [ ref_base_idx ];
                        have_mismatch = true;
#endif
                    handle_N_in_mismatch:
                        if ( ( et & PileupEvent :: insertion ) != 0 )
                        {
                            ++ ins_counts [ ref_base_idx ];
                            have_inserts = true;
                        }
                        break;

                    case PileupEvent :: mismatch:
                        mismatch = pileup . getAlignmentBase ();
                        mismatch_idx = 0;
                        switch ( toupper ( mismatch ) )
                        {
                        case 'A': break;
                        case 'C': mismatch_idx = 1; break;
                        case 'G': mismatch_idx = 2; break;
                        case 'T': mismatch_idx = 3; break;
                        default:
                            // treat N by removing this event from depth
                            -- depth;
                            goto handle_N_in_mismatch;
                        }

                        // going to reduce the mismatch index
                        // from 0..3 to 0..2

                        // first, assert that mismatch_idx cannot be ref_base_idx
                        assert ( mismatch_idx != ref_base_idx );

                        // count insertions
                        if ( ( et & PileupEvent :: insertion ) != 0 )
                            ++ ins_counts [ mismatch_idx ];
#if ! RECORD_MATCH_COUNT
                        // since we know the mimatch range is sparse,
                        // reduce it by 1 to make it dense
                        if ( mismatch_idx > ref_base_idx )
                            -- mismatch_idx;
#endif
                        // count the mismatches
                        ++ mismatch_counts [ mismatch_idx ];

                        have_mismatch = true;
                        break;

                    case PileupEvent :: deletion:
                        if ( pileup . getEventIndelType () == PileupEvent :: normal_indel )
                            ++ del_cnt;
                        else
                            -- depth;
                        break;
                    }
                }
            }

#if USE_GENERAL_LOADER
            // don't have to write RUN_NAME or REFERENCE_SPEC
            int64_t ref_pos = ref_zpos + 1;
#if RECORD_REF_POS
            out . write ( column_id [ col_REF_POS ], sizeof ref_pos * 8, & ref_pos, 1 );
#endif
#endif
            if ( depth > depth_cutoff )
            {
#if USE_GENERAL_LOADER
#if RECORD_REF_BASE
                out . write ( column_id [ col_REF_BASE ], sizeof ref_base * 8, & ref_base, 1 );
#endif
                out . write ( column_id [ col_DEPTH ], sizeof depth * 8, & depth, 1 );
                if ( have_mismatch )
                    out . write ( column_id [ col_MISMATCH_COUNTS ], sizeof mismatch_counts [ 0 ] * 8, mismatch_counts, 3 );
                if ( have_inserts )
                    out . write ( column_id [ col_INSERTION_COUNTS ], sizeof ins_counts [ 0 ] * 8, ins_counts, 4 );
                if ( del_cnt != 0 )
                    out . write ( column_id [ col_DELETION_COUNT ], sizeof del_cnt * 8, & del_cnt, 1 );
#else
                std :: cout
                    << runName
                    << '\t' << refName
                    << '\t' << ref_zpos + 1
                    << '\t' << ref_base
                    << '\t' << depth
                    << "\t{" << mismatch_counts [ 0 ]
                    << ',' << mismatch_counts [ 1 ]
                    << ',' << mismatch_counts [ 2 ]
                    << "}\t{" << ins_counts [ 0 ]
                    << ',' << ins_counts [ 1 ]
                    << ',' << ins_counts [ 2 ]
                    << ',' << ins_counts [ 3 ]
                    << "}\t" << del_cnt
                    << '\n'
                    ;
#endif
            }

#if USE_GENERAL_LOADER
            out . nextRow ( tbl_id );
#endif
#endif // NO_PILEUP_EVENTS

        }
    }

#if USE_GENERAL_LOADER
    static
    void prepareOutput ( GeneralWriter & out, const String & runName )
    {
        // add table
        tbl_id = out . addTable ( "STATS" );

        // add each column
        column_id [ col_RUN_NAME ] = out . addColumn ( tbl_id, "RUN_NAME", 8 );
        column_id [ col_REFERENCE_SPEC ] = out . addColumn ( tbl_id, "REFERENCE_SPEC", 8 );
#if RECORD_REF_POS
        column_id [ col_REF_POS ] = out . addColumn ( tbl_id, "REF_POS", 64, integer_column_flag_bits );
#endif
#if RECORD_REF_BASE
        column_id [ col_REF_BASE ] = out . addColumn ( tbl_id, "REF_BASE", 8 );
#endif
        column_id [ col_DEPTH ] = out . addColumn ( tbl_id, "DEPTH", 32, integer_column_flag_bits );
        column_id [ col_MISMATCH_COUNTS ] = out . addColumn ( tbl_id, "MISMATCH_COUNTS", 32, integer_column_flag_bits );
        column_id [ col_INSERTION_COUNTS ] = out . addColumn ( tbl_id, "INSERTION_COUNTS", 32, integer_column_flag_bits );
        column_id [ col_DELETION_COUNT ] = out . addColumn ( tbl_id, "DELETION_COUNT", 32, integer_column_flag_bits );

        // open the stream
        out . open ();

        // set default values
        out . columnDefault ( column_id [ col_RUN_NAME ], 8, runName . data (), runName . size () );
        out . columnDefault ( column_id [ col_DEPTH ], 32, "", 0 );
        out . columnDefault ( column_id [ col_MISMATCH_COUNTS ], 32, "", 0 );
        out . columnDefault ( column_id [ col_INSERTION_COUNTS ], 32, "", 0 );
        out . columnDefault ( column_id [ col_DELETION_COUNT ], 32, "", 0 );
    }
#endif

    static
    void run ( const char * spec, const char *outfile, const char *_remote_db, Alignment :: AlignmentCategory cat )
    {
        std :: cerr << "# Opening run '" << spec << "'\n";
        ReadCollection obj = ncbi :: NGS :: openReadCollection ( spec );
        String runName = obj . getName ();

#if USE_GENERAL_LOADER
        std :: cerr << "# Preparing version " << GW_CURRENT_VERSION << " pipe to stdout\n";
        if ( ( integer_column_flag_bits & 1 ) != 0 )
            std :: cerr << "#   USING INTEGER PACKING\n";
        std :: string remote_db;
        if ( _remote_db == NULL )
            remote_db = runName + ".pileup_stat";
        else
            remote_db = _remote_db;

        GeneralWriter *outp = ( outfile == NULL ) ? 
            new GeneralWriter ( 1 ) : new GeneralWriter ( outfile );

        try
        {
            GeneralWriter &out = *outp;

            // add remote db event
            out . setRemotePath ( remote_db );

            // use schema
            out . useSchema ( "align/pileup-stats.vschema", "NCBI:pileup:db:pileup_stats #1" );

            prepareOutput ( out, runName );
#endif
            std :: cerr << "# Accessing all references\n";
            ReferenceIterator ref = obj . getReferences ();
            
            while ( ref . nextReference () )
            {
                String refName = ref . getCanonicalName ();
                
                std :: cerr << "# Processing reference '" << refName << "'\n";
#if USE_GENERAL_LOADER
                out . columnDefault ( column_id [ col_REFERENCE_SPEC ], 8, refName . data (), refName . size () );
#endif

#if SLICE_WIDTH
                std :: cerr << "# Accessing " << SLICE_WIDTH << " pileups starting at " << SLICE_START << "\n";
                PileupIterator pileup = ref . getPileupSlice ( SLICE_START, SLICE_WIDTH, cat );
#else
                std :: cerr << "# Accessing all pileups\n";
                PileupIterator pileup = ref . getPileups ( cat );
#endif
#if USE_GENERAL_LOADER
                run ( out, runName, refName, pileup );
#else
                run ( runName, refName, pileup );
#endif
                if ( verbosity > 1 )
                    std :: cerr << '\n';
#if SINGLE_REFERENCE
                break;
#endif
            }

#if USE_GENERAL_LOADER
        }
        catch ( ErrorMsg & x )
        {
            outp -> logError ( x . what () );
            delete outp;
            throw;
        }
        catch ( const char x [] )
        {
            outp -> logError ( x );
            delete outp;
            throw;
        }
        catch ( ... )
        {
            outp -> logError ( "unknown exception" );
            delete outp;
            throw;
        }
        delete outp;
#endif
    }
}

extern "C"
{
    ver_t CC KAppVersion ()
    {
        return PILEUP_STATS_VERS;
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

        std :: cout
            << '\n'
            << "Usage:\n"
            << "  " << appLeaf << " [options] <accession>"
#if ! USE_GENERAL_LOADER
            << " [<accession> ...]"
#endif
            << "\n\n"
            << "Options:\n"
            << "  -o|--output-file                 file for output\n"
#if USE_GENERAL_LOADER
            << "                                   (default pipe to stdout)\n"
#endif
#if USE_GENERAL_LOADER
            << "  -r|--remote-db                   name of remote database to create\n"
            << "                                   (default <accession>.pileup_stat)\n"
#endif
            << "  -x|--depth-cutoff                cutoff for depth <= value (default 1)\n"
            << "  -a|--align-category              the types of alignments to pile up:\n"
            << "                                   { primary, secondary, all } (default all)\n"
#if USE_GENERAL_LOADER
            << "  -P|--pack-integer                pack integers in output pipe - uses less bandwidth\n"
#endif
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
        Alignment :: AlignmentCategory cat = Alignment :: all;

        try
        {
            int num_runs = 0;
            const char *outfile = NULL;
            const char *remote_db = NULL;

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
                case 'o':
                    outfile = findArg ( arg, i, argc, argv );
                    break;
#if USE_GENERAL_LOADER
                case 'r':
                    remote_db = findArg ( arg, i, argc, argv );
                    break;
#endif
                case 'x':
                    ncbi :: depth_cutoff = AsciiToU32 ( findArg ( arg, i, argc, argv ), 
                        handle_error, ( void * ) "Invalid depth cutoff" );
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
#if USE_GENERAL_LOADER
                case 'P':
                    ncbi :: integer_column_flag_bits = 1;
                    break;
#endif
                case 'v':
                    ++ ncbi :: verbosity;
                    break;
                case 'h':
                case '?':
                    handle_help ( argv [ 0 ] );
                    return 0;
                case '-':
                    ++ arg;
                    if ( strcmp ( arg, "output-file" ) == 0 )
                    {
                        outfile = getArg ( i, argc, argv );
                    }
#if USE_GENERAL_LOADER
                    else if ( strcmp ( arg, "remote-db" ) == 0 )
                    {
                        remote_db = getArg ( i, argc, argv );
                    }
#endif
                    else if ( strcmp ( arg, "depth-cutoff" ) == 0 )
                    {
                        ncbi :: depth_cutoff = AsciiToU32 ( getArg ( i, argc, argv ), 
                            handle_error, ( void * ) "Invalid depth cutoff" );
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
#if USE_GENERAL_LOADER
                    else if ( strcmp ( arg, "pack-integer" ) == 0 )
                    {
                        ncbi :: integer_column_flag_bits = 1;
                    }
#endif
                    else if ( strcmp ( arg, "verbose" ) == 0 )
                    {
                        ++ ncbi :: verbosity;
                    }
                    else if ( strcmp ( arg, "help" ) == 0 )
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

#if USE_GENERAL_LOADER
            if ( num_runs > 1 )
                throw "only one run may be processed at a time";
#endif
            for ( int i = 1; i <= num_runs; ++ i )
            {
                ncbi :: run ( argv [ i ], outfile, remote_db, cat );
            }

            rc = 0;
        }
        catch ( ErrorMsg & x )
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
