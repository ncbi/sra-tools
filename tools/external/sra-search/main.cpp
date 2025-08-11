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

#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <map>
#include <sstream>

#include <strtol.h>

#include <kapp/main.h>
#include <kfg/config.h> /* KConfigSetNgcFile */

#include "vdb-search.hpp"

using namespace std;

const char UsageDefaultName[] = "sra-search";

struct FragmentId_Less
{
    bool operator() ( const string& p_a, const string& p_b ) const
    {
        string accA;
        uint32_t fragA;
        int64_t readA;
        Parse ( p_a, accA, fragA, readA );
        string accB;
        uint32_t fragB;
        int64_t readB;
        Parse ( p_b, accB, fragB, readB );
        if ( accA == accB )
        {
            if ( readA == readB )
            {
                return fragA < fragB;
            }
            return readA < readB;
        }
        return accA < accB;
    }

    static void Parse ( const string& p_id, string& p_acc, uint32_t& p_frag, int64_t& p_read )
    {
        size_t firstDot = p_id . find ( ".FR", 0 );
        if ( firstDot == string :: npos )
        {
            p_acc = p_id;
            p_frag = 0;
            p_read = 0;
        }
        else
        {
            p_acc = p_id . substr ( 0, firstDot );
            size_t secondDot = p_id . find ( '.', firstDot + 3 );
            if ( secondDot == string :: npos )
            {
                p_frag = strtol ( p_id . substr ( firstDot + 3 ) . c_str (), NULL, 10 );
                p_read = 0;
            }
            else
            {
                p_frag = strtol ( p_id . substr ( firstDot + 3, secondDot - firstDot - 3 ) . c_str (), NULL, 10 );
                p_read = strtol ( p_id . substr ( secondDot + 1 ) . c_str (), NULL, 10 );
            }
        }
    }
};

typedef map < string, string, FragmentId_Less > Results;

static
bool
DoSearch ( const VdbSearch :: Settings& p_settings, bool p_sortOutput  )
{
    VdbSearch s ( p_settings );

    string acc;
    string fragId;
    bool ret = false;
    if ( p_sortOutput )
    {
        Results results;
        while ( true )
        {
            VdbSearch :: Match m;
            if ( ! s . NextMatch ( m ) )
            {
                break;
            }
            results [ m . m_fragmentId ] = m . m_formatted;
        }
        for ( Results::const_iterator i = results.begin(); i != results.end(); ++i)
        {
            cout << i -> second << endl;
        }
        ret = results . size () > 0;
    }
    else
    {
        while ( true )
        {
            VdbSearch :: Match m;
            if ( ! s . NextMatch ( m ) )
            {
                break;
            }
            cout << m . m_formatted << endl;
            ret = true;
        }
    }
    return ret;
}

static void handle_help ( const char * appName )
{
    string fileName = appName;
    string::size_type filePos = fileName . rfind ( '/' );
    if ( filePos != string::npos)
    {
        fileName = fileName . substr ( filePos + 1 );
    }

    cout << endl
        << "Usage:" << endl
        << "  " << fileName << " [Options] query accession ..." << endl
        << endl
        << "Summary:" << endl
        << "  Searches all reads in the accessions and prints Ids of all the fragments that contain a match." << endl
        << endl
        << "Example:" << endl
        << "  sra-search ACGT SRR000001 SRR000002" << endl
        << "  sra-search \"CGTA||ACGT\" -e -a NucStrstr SRR000002" << endl
        << endl
        << "Options:" << endl
        << "  -h|--help                 Output brief explanation of the program." << endl
        << "  -a|--algorithm <alg>      Search algorithm, one of:" << endl
        ;

    const VdbSearch :: SupportedAlgorithms algs = VdbSearch :: GetSupportedAlgorithms ();
    for ( VdbSearch :: SupportedAlgorithms :: const_iterator i = algs . begin (); i != algs . end (); ++i )
    {
        cout << "      " << *i;
        if ( i == algs . begin () )
        {
            cout << " (default)";
        }
        cout << endl;
    }
    cout << "  -e|--expression <expr>    Query is an expression (currently only supported for NucStrstr)" << endl
         << "  -S|--score <number>       Minimum match score (0..100), default 100 (perfect match);" << endl
         << "                            supported for all variants of Agrep and SmithWaterman." << endl
         << "  -T|--threads <number>     The number of threads to use; 2 by deafult" << endl
         << "  --threadperacc            One thread per accession mode (by default, multiple threads per accession)" << endl
         << "  --sort                    Sort output by accession/read/fragment" << endl
         << "  --reference [refName,...] Scan reference(s) for potential matches; all references if none specified" << endl
         << "  -m|--max <number>         Stop after N matches" << endl
         << "  -U|--unaligned            Search in unaligned and partially aligned reads only" << endl
         << "  --fasta [ <lineWidth> ]   Output in FASTA format with specified line width (default 70 bases)" << endl
         ;

    cout << endl;

    HelpVersion ( UsageDefaultName, KAppVersion () );
}

int
run( int argc, char *argv [] )
{
    int rc = -1;
    bool found;

    try
    {
        VdbSearch :: Settings settings;
        bool sortOutput = false;

        int i = 1;
        while ( i < argc )
        {
            string arg = argv [ i ];
            if ( arg [ 0 ] != '-' )
            {
                if ( settings . m_query . empty () )
                {
                    settings . m_query = arg;
                }
                else
                {   // an input run
                    settings . m_accessions . push_back ( arg );
                }
            }
            else if ( arg == "-h" || arg == "--help" )
            {
                handle_help ( argv [ 0 ]  );
                return 0;
            }
            else if ( arg == "-V"  || arg == "--version" )
            {
                HelpVersion ( UsageDefaultName, KAppVersion () );
                return 0;
            }
            else if ( arg == "-a" || arg == "--algorithm" )
            {
                ++i;
                if ( i >= argc )
                {
                    throw invalid_argument ( string ( "Missing argument for " ) + arg );
                }
                if ( ! settings . SetAlgorithm ( argv [ i ] ) )
                {
                    throw invalid_argument ( string ( "unrecognized algorithm: " ) + argv [ i ] );
                }
            }
            else if ( arg == "-e" || arg == "--expression" )
            {
                settings . m_isExpression = true;
            }
            else if ( arg == "-S" || arg == "--score" )
            {
                ++i;
                if ( i >= argc )
                {
                    throw invalid_argument ( string ( "Missing argument for " ) + arg );
                }
                char* endptr;
                int32_t score = strtoi32 ( argv [ i ], &endptr, 10 );
                if ( *endptr != 0 || score <= 0 || errno == ERANGE )
                {
                    throw invalid_argument ( string ( "Invalid argument for " ) + arg + ": '" + argv [ i ] + "'");
                }
                settings . m_minScorePct = ( unsigned int ) score;
            }
            else if ( arg == "-T" || arg == "--threads" )
            {
                ++i;
                if ( i >= argc )
                {
                    throw invalid_argument ( string ( "Missing argument for " ) + arg );
                }
                char* endptr;
                int32_t threads = strtoi32 ( argv [ i ], &endptr, 10 );
                if ( *endptr != 0 || threads <= 0 || errno == ERANGE )
                {
                    throw invalid_argument ( string ( "Invalid argument for " ) + arg + ": '" + argv [ i ] + "'");
                }
                settings . m_threads = ( unsigned int ) threads;
            }
            else if ( arg == "--threadperacc" )
            {
                settings . m_threadPerAcc = true;
            }
            else if ( arg == "--sort" )
            {
                sortOutput = true;
            }
            else if ( arg == "--reference" )
            {
                settings . m_referenceDriven = true;
                if ( i + 1 < argc && argv [ i + 1 ] [ 0 ] != '-' )
                {   // parse reference specs
                    istringstream str ( argv [ i + 1 ] );
                    string name;
                    while ( getline ( str, name, ',') )
                    {
                        settings . m_references . push_back ( name );
                    }
                    ++i;
                }
            }
            else if ( arg == "-m" || arg == "--max" )
            {
                ++i;
                if ( i >= argc )
                {
                    throw invalid_argument ( string ( "Missing argument for " ) + arg );
                }
                char* endptr;
                int32_t maxMatches = strtoi32 ( argv [ i ], &endptr, 10 );
                if ( *endptr != 0 || maxMatches <= 0 || errno == ERANGE )
                {
                    throw invalid_argument ( string ( "Invalid argument for " ) + arg + ": '" + argv [ i ] + "'");
                }
                settings . m_maxMatches = ( unsigned int ) maxMatches;
            }
            else if ( arg == "-U" || arg == "--unaligned" )
            {
                settings . m_unaligned = true;
            }
            else if ( arg == "--fasta" )
            {
                if ( i + 1 < argc )
                {   // check for an integer parameter
                    char* endptr;
                    int32_t lineLength = strtoi32 ( argv [ i + 1 ], &endptr, 10 );
                    if ( *endptr == 0 )
                    {
                        if ( lineLength <= 0 )
                        {
                            throw invalid_argument ( string ( "Invalid argument for " ) + arg + ": '" + argv [ i ] + "' (has to be a positive integer)");
                        }
                        settings . m_fastaLineLength = lineLength;
                        ++ i;
                    }
                }
                settings . m_fasta = true;
            }
            else if ( arg == "--ngc" )
            {
                ++i;
                if (i >= argc)
                {
                    throw invalid_argument (
                        string ( "Missing argument for ") + arg );
                }
                KConfigSetNgcFile ( argv [ i ] );
            }
            else
            {
                throw invalid_argument ( string ( "Invalid option " ) + arg );
            }

            ++i;
        }

        if ( settings . m_query . empty () || settings . m_accessions . size () == 0 )
        {
            throw invalid_argument ( "Missing arguments" );
        }
        if ( settings . m_referenceDriven && settings . m_isExpression )
        {
            throw invalid_argument ( "Options --reference and --expression cannot be used together" );
        }

        found = DoSearch ( settings, sortOutput );

        rc = 0;
    }
    catch ( const invalid_argument & x )
    {
        cerr << endl << "ERROR: " << x . what () << endl;
        handle_help ( argv [ 0 ] );
        rc = 1;
    }
    catch ( const exception & x )
    {
        cerr << endl << "ERROR: " << argv [ 0 ] << ": " << x . what () << endl;
        rc = 2;
    }
    catch ( ... )
    {
        cerr << endl << "ERROR: "<< argv [ 0 ] << ": unknown" << endl;
        rc = 3;
    }

    if ( rc == 0 && ! found )
    {
        rc = 1;
    }

    return rc;
}

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);
    app.setRc( run ( argc, app.getArgV()) );
    return app.getExitCode();
}