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

#include <ncbi/NGS.hpp> // openReadCollection
#include <vector>

#include <kapp/main.h>

using std::cerr;
using std::cout;
using std::endl;

const char UsageDefaultName[] = "compute-coverage-ngs";

////////////////////////////////////////////////////////////////////////////////

static
void handle_help ()
{
    std :: cout
        << '\n'
        << "Usage:\n"
        << "  " << UsageDefaultName << " [options] <accession>"
        << "\n\n"
        << "Options:\n"
        << "  -h|--help                        Output brief explanation for the program. \n"
        << "  -V|--version                     Display the version of the program then\n"
        << "                                   quit.\n"
        ;
    HelpVersion ( UsageDefaultName, KAppVersion () );
}

int run ( int argc, char * argv [] ) {
    bool TESTING = getenv ( "VDB_TEST" ) != NULL;
//    const char * accession ( "SRR543323" );
   const char * accession = 0;

    for ( int i = 1; i < argc; ++ i )
    {
        const char * arg = argv [ i ];
        if ( arg [ 0 ] != '-' )
        {
            if ( accession != 0 )
            {
                cerr << "More than one accession specified" << endl;
                return EXIT_FAILURE;
            }
            accession = argv [ i ];
        }
        else
        {
            switch ( arg [ 1 ] )
            {
            case 'h':
                handle_help ();
                return 0;
            case 'V':
                HelpVersion ( UsageDefaultName, KAppVersion () );
                return 0;
            case '-':
                if ( strcmp ( & arg [ 2 ], "help"  ) == 0 )
                {
                    handle_help ();
                    return 0;
                }
                else if ( strcmp ( & arg [ 2 ], "version"  ) == 0 )
                {
                    HelpVersion ( UsageDefaultName, KAppVersion () );
                    return 0;
                }
                else
                {
                    cerr << "Invalid argument '" << & arg [ 2 ] << "'" << endl;
                    return 1;
                }
                break;
            default:
                cerr << "Invalid argument '" << & arg [ 1 ] << "'" << endl;
                return 1;
            }
        }
    }

    try {
        ngs::ReadCollection run ( ncbi::NGS::openReadCollection ( accession ) );

        ngs::ReferenceIterator ri ( run . getReferences () );
        while ( ri . nextReference () ) {
            ngs::PileupIterator pi
                ( ri . getPileups ( ngs::Alignment::primaryAlignment ) );

            int num = 0;
            typedef std::vector < int > TVector;
            TVector rc;
            TVector::size_type max = 0;

            int pos = 0;
            while ( pi . nextPileup () ) {
                uint32_t depth ( pi . getPileupDepth () );
                if ( depth > 0 ) {
                    if ( max < depth ) {
                        rc . resize ( depth + 1 );
                        max = depth;
                    }
                    rc [ depth ] ++;
                    ++ num;
                }
                ++pos;
            }

            int64_t q [ 5 ];
            for ( unsigned i = 0; i < sizeof q / sizeof q [ 0 ]; ++ i )
                q [ i ] = -1;

            for ( TVector::size_type i = 1, c = 0;
                  i < rc . size (); ++ i )
            {
                c += rc [ i ];

                if ( q [ 0 ] == -1 && .10 * num < c )
                     q [ 0 ] = i;
                if ( q [ 1 ] == -1 && .25 * num < c )
                     q [ 1 ] = i;
                if ( q [ 2 ] == -1 && .50 * num < c )
                     q [ 2 ] = i;
                if ( q [ 3 ] == -1 && .75 * num < c )
                     q [ 3 ] = i;
                if ( q [ 4 ] == -1 && .90 * num < c ) {
                     q [ 4 ] = i;
                     break;
                }
            }

            cout << ri . getCanonicalName ();
            for ( unsigned i = 0; i < sizeof q / sizeof q [ 0 ]; ++ i ) {
                if ( q [ i ] == -1 )
                    break;
                cout <<  "\t" << q [ i ];
            }
            if ( TESTING ) {
                cout <<  "\t" << pos << "|" << num << "|" << max;
                for ( unsigned i = 1; i <= max; ++ i )
                    cout <<  "|" << rc [ i ];
            }
            cout << endl;
        }

        return EXIT_SUCCESS;
    }

    catch ( ngs::ErrorMsg & e ) {
        cerr << "Error: " << e . toString () << endl;
    }
    catch ( std::exception & e ) {
        cerr << "Error: " << e . what () << endl;
    }
    catch ( ... ) {
        cerr << "Error: unknown exception" << endl;
    }

    return EXIT_FAILURE;
}

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);
    return run ( argc, app.getArgV() );
}
