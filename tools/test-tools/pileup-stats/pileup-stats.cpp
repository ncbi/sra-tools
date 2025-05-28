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

#include <general-writer/general-writer.hpp>

#include <ncbi/NGS.hpp>
#include <ngs-sdk/ngs/ReadCollection.hpp>
#include <ngs-sdk/ngs/Reference.hpp>
#include <ngs-sdk/ngs/Alignment.hpp>
#include <ngs-sdk/ngs/Pileup.hpp>
#include <ngs-sdk/ngs/PileupEvent.hpp>

#include <kapp/main.h>
#include <klib/printf.h>
#include <klib/sra-release-version.h>
#include <iomanip>

#define DFLT_BUFFER_SIZE ( 32 * 1024 )

#include <arch-impl.h>

#include <iostream>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <unistd.h>

using namespace ngs;

namespace ncbi
{
    enum
    {
        col_RUN_NAME,
        col_REFERENCE_SPEC,
        col_REF_POS_TRANS,
        col_DEPTH,
        col_MISMATCH_COUNTS,
        col_INSERTION_COUNTS,
        col_DELETION_COUNT,

        num_columns
    };

    static int tbl_id;
    static int column_id [ num_columns ];
    static uint8_t integer_column_flag_bits = 1; // pack integers by default
    static int64_t zrow_id;

    static uint32_t depth_cutoff = 1;               // do not output if depth <= this value
    static uint32_t event_cutoff = 1;
    static uint32_t num_significant_bits = 4;

    static uint32_t verbosity;
    static bool use_gw_logmessage = true; // log via general-writer API rather than stderr

    const bool need_write_true = false;

    inline
    uint32_t filter_significant_bits ( uint32_t val )
    {
        if ( val != 0 )
        {
            // we find the most significant bit in value
            // which will be 0..31 for non-zero values
            // from which we subtract the number of significant bits
            // being retained ( - 1 to account for 0-based index )
            //
            // e.g.:
            //  val = 0x31, num_significant_bits = 4
            //  uint32_msbit ( val ) => 5, meaning the most significant bit is 5,
            //  which says there are 6 significant bits in total. we only want to
            //  keep 4, however.
            //  mask_index = 5 - ( 4 - 1 ) => 2
            //  0xFFFFFFFF << mask_index => 0xFFFFFFFC
            //  val then becomes 0x30
            int mask_index = uint32_msbit ( val ) - ( int ) ( num_significant_bits - 1 );
            if ( mask_index > 0 )
            {
                // this is a positive quantity
                assert ( mask_index <= 31 );
                val = val & ( 0xFFFFFFFF << mask_index );
            }
        }

        return val;
    }

    static
    void run (
        GeneralWriter & out,
        const String & runName, const String & refName, PileupIterator & pileup, uint64_t refLength, uint64_t refLengthSubTotal, uint64_t refLengthTotal )
    {
        int64_t ref_zpos, last_writ = 0;
        bool need_write = false;

        for ( ref_zpos = -1; pileup . nextPileup (); ++ ref_zpos, ++ zrow_id )
        {
            if ( ref_zpos < 0 )
            {
                last_writ = ref_zpos = pileup . getReferencePosition ();
                int64_t ref_pos_trans = ref_zpos - zrow_id;
                out . columnDefault ( column_id [ col_REF_POS_TRANS ], 64, & ref_pos_trans, 1 );
                need_write = need_write_true;
            }

            switch ( verbosity )
            {
            case 0:
                break;
            case 1:
                if ( ( ref_zpos % 1000000 ) == 0 )
                {
                    if ( ncbi :: use_gw_logmessage )
                        out . progMsg ( refName, KAppVersion(), ref_zpos + refLengthSubTotal , refLengthTotal );
                    else
                        std :: cerr << "#  " << std :: setw ( 9 ) << ref_zpos << '\n';
                }
                break;
            default:
                if ( ( ref_zpos % 5000 ) == 0 )
                {
                    if ( ncbi :: use_gw_logmessage )
                        out . progMsg ( refName, KAppVersion(), ref_zpos + refLengthSubTotal , refLengthTotal );
                    else
                    {
                        if ( ( ref_zpos % 500000 ) == 0 )
                            std :: cerr << "\n#  " << std :: setw ( 9 ) << ref_zpos << ' ';
                        std :: cerr << '.';
                    }
                }
            }

            uint32_t ref_base_idx = 0;
            bool ref_ambiguous = false;
            char ref_base = pileup . getReferenceBase ();
            switch ( toupper ( ref_base ) )
            {
            case 'A': break;
            case 'C': ref_base_idx = 1; break;
            case 'G': ref_base_idx = 2; break;
            case 'T': ref_base_idx = 3; break;
            default:
                ref_base_idx = 0;
                ref_ambiguous = true;
            }

            uint32_t depth = pileup . getPileupDepth ();

            uint32_t mismatch_counts [ 4 ];
            memset ( mismatch_counts, 0, sizeof mismatch_counts );

            uint32_t ins_counts [ 4 ];
            memset ( ins_counts, 0, sizeof ins_counts );

            uint32_t del_cnt = 0;

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

                        if ( ! ref_ambiguous )
                        {
                    handle_N_in_mismatch:
                            if ( ( et & PileupEvent :: insertion ) != 0 )
                                ++ ins_counts [ ref_base_idx ];
                            break;
                        }
                        // intentional fall-through

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

                        // first, assert that mismatch_idx cannot be ref_base_idx
                        assert ( ref_ambiguous || mismatch_idx != ref_base_idx );

                        // count insertions
                        if ( ( et & PileupEvent :: insertion ) != 0 )
                            ++ ins_counts [ mismatch_idx ];
                        // count the mismatches
                        ++ mismatch_counts [ mismatch_idx ];
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

            if ( depth > depth_cutoff )
            {
                int i;

                if ( del_cnt <= event_cutoff )
                    del_cnt = 0;

                bool have_mismatch = false;
                for ( i = 0; i < 4; ++ i )
                {
                    if ( mismatch_counts [ i ] <= event_cutoff )
                        mismatch_counts [ i ] = 0;
                    else
                        have_mismatch = true;
                }

                bool have_inserts = false;
                for ( i = 0; i < 4; ++ i )
                {
                    if ( ins_counts [ i ] <= event_cutoff )
                        ins_counts [ i ] = 0;
                    else
                        have_inserts = true;
                }

                if ( num_significant_bits != 0 )
                {
                    depth = filter_significant_bits ( depth );
                    del_cnt = filter_significant_bits ( del_cnt );
                    for ( i = 0; i < 4; ++ i )
                        mismatch_counts [ i ] = filter_significant_bits ( mismatch_counts [ i ] );
                    for ( i = 0; i < 4; ++ i )
                        ins_counts [ i ] = filter_significant_bits ( ins_counts [ i ] );
                }


                if ( ref_zpos > last_writ )
                    out . moveAhead ( tbl_id, ref_zpos - last_writ );
                out . write ( column_id [ col_DEPTH ], sizeof depth * 8, & depth, 1 );
                if ( have_mismatch )
                    out . write ( column_id [ col_MISMATCH_COUNTS ], sizeof mismatch_counts [ 0 ] * 8, mismatch_counts, 4 );
                if ( have_inserts )
                    out . write ( column_id [ col_INSERTION_COUNTS ], sizeof ins_counts [ 0 ] * 8, ins_counts, 4 );
                if ( del_cnt != 0 )
                    out . write ( column_id [ col_DELETION_COUNT ], sizeof del_cnt * 8, & del_cnt, 1 );
                out . nextRow ( tbl_id );
                last_writ = ref_zpos + 1;
            }
            else if ( need_write )
            {
                out . nextRow ( tbl_id );
                need_write = false;
            }

        }
        if ( ref_zpos > last_writ )
            out . moveAhead ( tbl_id, ref_zpos - last_writ );
    }

    static
    void prepareOutput ( GeneralWriter & out, const String & runName )
    {
        // add table
        tbl_id = out . addTable ( "STATS" );

        // add each column
        column_id [ col_RUN_NAME ] = out . addColumn ( tbl_id, "RUN_NAME", 8 );
        column_id [ col_REFERENCE_SPEC ] = out . addColumn ( tbl_id, "REFERENCE_SPEC", 8 );
        column_id [ col_REF_POS_TRANS ] = out . addColumn ( tbl_id, "REF_POS_TRANS", 64, 0 );
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

    static
    void set_software ( GeneralWriter & out )
    {
        char vers_str [ 64 ];
        ver_t version = KAppVersion ();
        string_printf ( vers_str, sizeof vers_str, NULL, "%V", version );
        out . setSoftwareName ( "pileup-stats", vers_str );
    }

    static
    void run ( const char * spec, const char *outfile, const char *_remote_db, size_t buffer_size, Alignment :: AlignmentCategory cat )
    {
        if ( ! ncbi :: use_gw_logmessage )
        {
            if ( verbosity > 0 )
                std :: cerr << "# Opening run '" << spec << "'\n";
        }
        ReadCollection obj = ncbi :: NGS :: openReadCollection ( spec );
        String runName = obj . getName ();

        if ( ! ncbi :: use_gw_logmessage )
        {
            if ( verbosity > 0 )
            {
                std :: cerr << "# Preparing version " << GW_CURRENT_VERSION << " pipe to stdout\n";
                if ( ( integer_column_flag_bits & 1 ) != 0 )
                    std :: cerr << "#   USING INTEGER PACKING\n";
            }
        }

        std :: string remote_db;
        if ( _remote_db == NULL )
            remote_db = runName + ".pileup_stat";
        else
            remote_db = _remote_db;

        GeneralWriter *outp = ( outfile == NULL ) ?
            new GeneralWriter ( 1, buffer_size ) : new GeneralWriter ( outfile );

        try
        {
            GeneralWriter &out = *outp;

            // set software node
            set_software ( out );

            // add remote db event
            out . setRemotePath ( remote_db );

            // use schema
            out . useSchema ( "align/pileup-stats.vschema", "NCBI:pileup:db:pileup_stats #1" );

            prepareOutput ( out, runName );
            if ( verbosity > 0 )
            {
                if ( ncbi :: use_gw_logmessage )
                    out . logMsg ( "Accessing all references" );
                else
                    std :: cerr << "# Accessing all references\n";
            }



            // get the total number of references
            uint64_t totalRefLength = 0;
            {
                ReferenceIterator rf = obj . getReferences ();
                while ( rf . nextReference () )
                    totalRefLength += rf . getLength ();
            }

            ReferenceIterator ref = obj . getReferences ();

            uint64_t refLengthSubTotal = 0;
            while ( ref . nextReference () )
            {
                String refName = ref . getCanonicalName ();

                //get the length of the reference so that progress can be given
                uint64_t refLength = ref . getLength ();

                if ( verbosity > 0 )
                {
                    if ( ncbi :: use_gw_logmessage )
                        out . logMsg ( "Processing reference '" + refName + "'" );
                    else
                        std :: cerr << "# Processing reference '" << refName << "'\n";
                }
                out . columnDefault ( column_id [ col_REFERENCE_SPEC ], 8, refName . data (), refName . size () );

                if ( verbosity > 0 )
                {
                    if ( ncbi :: use_gw_logmessage )
                        out . logMsg ( "Accessing all pileups" );
                    else
                        std :: cerr << "# Accessing all pileups\n";
                }
                PileupIterator pileup = ref . getPileups ( cat );
                run ( out, runName, refName, pileup, refLength, refLengthSubTotal,  totalRefLength );

                refLengthSubTotal += refLength;
                if ( ! ncbi :: use_gw_logmessage )
                {
                    if ( verbosity > 1 )
                        std :: cerr << '\n';
                }
            }
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
    }
}

extern "C"
{
    const char UsageDefaultName [] = "pileup-stats";

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
            << "\n\n"
            << "Options:\n"
            << "  -o|--output-file                 file for output\n"
            << "                                   (default pipe to stdout)\n"
            << "  -r|--remote-db                   name of remote database to create\n"
            << "                                   (default <accession>.pileup_stat)\n"
            << "  -x|--depth-cutoff                cutoff for depth <= value (default 1)\n"
            << "  -e|--event-cutoff                cutoff for number of events > value (default " << ncbi::event_cutoff << ")\n"
            << "  -p|--num-significant-bits        number of significant bits for depth and counts to store (default " << ncbi::num_significant_bits << ")\n"
            << "  -a|--align-category              the types of alignments to pile up:\n"
            << "                                   { primary, secondary, all } (default primary)\n"
            << "  --buffer-size bytes              size of output pipe buffer - default " << DFLT_BUFFER_SIZE/1024 << "K bytes\n"
            << "  -U|--unpack-integer              don't pack integers in output pipe - uses more bandwidth\n"
            << "  -h|--help                        output brief explanation of the program\n"
            << "  -v|--verbose                     increase the verbosity of the program.\n"
            << "  -V|--version                     display the version of the program then quit.\n"
            << "                                   use multiple times for more verbosity.\n"
            << "  --log-stderr                     log via stderr rather than general-writer API (default - general-writer API)\n"
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

    static void handle_version ( const char *progname )
    {
        ::ver_t vers = ::KAppVersion();

        HelpVersion ( progname, vers );
    }

    static void CC handle_error ( const char *arg, void *message )
    {
        throw ( const char * ) message;
    }

    rc_t CC UsageSummary ( const char * progname ) {
        handle_help ( progname );
        return 0;
    }

    MAIN_DECL(argc, argv)
    {
        VDB::Application app( argc, argv );
        if (!app)
        {
            return VDB_INIT_FAILED;
        }

        rc_t rc = -1;
        Alignment :: AlignmentCategory cat = Alignment :: primaryAlignment;
        size_t buffer_size = DFLT_BUFFER_SIZE;

        SetUsageSummary( UsageSummary );

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
                case 'h':
                case '?':
                    handle_help ( argv [ 0 ] );
                    return 0;
                case 'V':
                    handle_version ( argv [ 0 ] );
                    return 0;
                case 'L': // eat it and do nothing
                    findArg ( arg, i, argc, argv );
                    break;
                case 'z': // eat it and do nothing
                    findArg ( arg, i, argc, argv );
                    break;
                case '-':
                    ++ arg;
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
                    else if ( strcmp ( arg, "log-stderr" ) == 0 )
                    {
                        ncbi :: use_gw_logmessage = false;
                    }
                    else if ( strcmp ( arg, "help" ) == 0 )
                    {
                        handle_help ( argv [ 0 ] );
                        return 0;
                    }
                    else if ( strcmp ( arg, "version" ) == 0 )
                    {
                        handle_version ( argv [ 0 ] );
                        return 0;
                    }
                    else if ( strcmp ( arg, "log-level" ) == 0 )
                    {
                        /* eat it and do nothing */
                        getArg ( i, argc, argv );
                    }
                    else if ( strcmp ( arg, "xml-log" ) == 0 )
                    {
                        /* eat it and do nothing */
                        getArg ( i, argc, argv );
                    }
                    else if ( strcmp ( arg, "xml-log-fd" ) == 0 )
                    {
                        /* eat it and do nothing */
                        getArg ( i, argc, argv );
                    }
                    else if ( strcmp ( arg, "version" ) == 0 )
                    {
                        handle_version ( argv [ 0 ] );
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

            if ( num_runs == 0 )
            {
                handle_help ( argv [ 0 ] );
                throw "no runs specified";
            }
            else if ( num_runs > 1 )
            {
                handle_help ( argv [ 0 ] );
                throw "only one run may be processed at a time";
            }

            for ( int i = 1; i <= num_runs; ++ i )
            {
                ncbi :: run ( argv [ i ], outfile, remote_db, buffer_size, cat );
            }

            rc = 0;
        }
        catch ( ErrorMsg & x )
        {
            if ( ! ncbi :: use_gw_logmessage )
            {
                std :: cerr
                    << "ERROR: "
                    << argv [ 0 ]
                    << ": "
                    << x . what ()
                    << '\n'
                    ;
            }
        }
        catch ( const char x [] )
        {
            if ( ! ncbi :: use_gw_logmessage )
            {
                std :: cerr
                    << "ERROR: "
                    << argv [ 0 ]
                    << ": "
                    << x
                    << '\n'
                    ;
            }
        }
        catch ( ... )
        {
            if ( ! ncbi :: use_gw_logmessage )
            {
                std :: cerr
                    << "ERROR: "
                    << argv [ 0 ]
                    << ": unknown\n"
                    ;
            }
        }

        app.setRc( rc );
        return app.getExitCode();
    }
}
