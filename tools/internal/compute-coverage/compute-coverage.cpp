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

#include <align/reference.h> // ReferenceList_Release
#include <align/unsupported_pileup_estimator.h> // ReleasePileupEstimator

#include <klib/rc.h>
#include <kapp/main.h>

#include <iostream> // cout
#include <cstdlib> // EXIT_SUCCESS

using std::cerr;
using std::cout;
using std::endl;

const char UsageDefaultName[] = "compute-coverage";

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

static rc_t PileupEstimatorRelease ( PileupEstimator * self ) {
    return ReleasePileupEstimator ( self );
}

static rc_t ReferenceListRelease ( const ReferenceList * self ) {
    ReferenceList_Release ( self );
    return 0;
}

static rc_t ReferenceObjRelease ( const ReferenceObj * self ) {
    ReferenceObj_Release ( self );
    return 0;
}

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
                    return true;
                }
                else
                {
                    cerr << "Invalid argument '" << & arg [ 2 ] << "'" << endl;
                    return EXIT_FAILURE;
                }
                break;
            default:
                cerr << "Invalid argument '" << & arg [ 1 ] << "'" << endl;
                return EXIT_FAILURE;
            }
        }
    }

    const VDBManager * mgr = NULL;
    rc_t rc = VDBManagerMakeRead ( & mgr, NULL );
    if ( rc != 0 )
        cerr << rc << " while calling VDBManagerMakeRead()" << endl;
    const ReferenceList * rl = NULL;
    if ( rc == 0 ) {
        rc = ReferenceList_MakePath ( & rl, mgr, accession, 0, 0, NULL, 0 );
        if ( rc != 0 )
            cerr << rc << " while calling ReferenceList_MakePath"
                                          "(" << accession << ")" << endl;
    }

    PileupEstimator * pe = NULL;
    if ( rc == 0 ) {
        rc = MakePileupEstimator ( & pe, accession, 0, NULL, NULL, 0, true );
        if ( rc != 0 )
            cerr << rc << " while calling MakePileupEstimator"
                                          "(" << accession << ")" << endl;
    }

    uint32_t count = 0;
    if ( rc == 0 ) {
        rc = ReferenceList_Count ( rl, & count );
        if ( rc != 0 )
            cerr << rc << " while calling ReferenceList_Count"
                                          "(" << accession << ")" << endl;
    }

	unsigned ALLOCATED = 100;
    int * RC = static_cast < int * > ( calloc ( ALLOCATED, sizeof RC [ 0 ] ) );
    if ( RC == NULL ) {
        rc = RC ( rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted );
        cerr << "Out of memory" << endl;
    }

    for ( uint32_t idx = 0; rc == 0 && idx < count; ++ idx ) {
		unsigned MAX = 0;
        int NUM = 0;

        const ReferenceObj * obj = NULL;
        rc = ReferenceList_Get ( rl, & obj, idx );
        if ( rc != 0 ) {
            cerr << rc << " while calling ReferenceList_Get"
                                 "(" << accession << ", " << idx << ")" << endl;
            break;
        }

        const char * name = NULL;
        rc = ReferenceObj_SeqId ( obj, & name );
        if ( rc != 0 ) {
            cerr << rc << " while calling ReferenceObj_SeqId"
                                 "(" << accession << ", " << idx << ")" << endl;
            break;
        }

        String refname;
        StringInitCString ( & refname, name );

        INSDC_coord_len len = 0;
        rc = ReferenceObj_SeqLength ( obj, & len );
        if ( rc != 0 ) {
            cerr << rc << " while calling ReferenceObj_SeqLength"
               "(" << accession << ", " << idx << " (" << name << ") )" << endl;
            break;
        }

        RELEASE ( ReferenceObj, obj );

        uint32_t coverage [ 5000 ];
        INSDC_coord_len slice_start = 0;
        for ( slice_start = 0; slice_start < len; ) {
            uint32_t slice_len = sizeof coverage / sizeof coverage [ 0 ];
            if ( slice_start + slice_len > len )
                slice_len = len - slice_start;
            rc = RunCoverage ( pe, & refname,
                               slice_start, slice_len, coverage );
            if ( rc != 0 ) {
                cerr << rc << " while calling RunCoverage"
                  "(" << accession << ", " << name << ", "
                      << slice_start << ", " << slice_len << ")" << endl;
                break;
            }

            for ( unsigned sid = 0; sid < slice_len; ++ sid ) {
                if ( coverage [ sid ] != 0 ) {
                    uint32_t depth = coverage [ sid ];
                    -- depth;
                    if ( MAX < depth + 1 ) {
                        MAX = depth + 1;
                        if ( ALLOCATED < MAX ) {
                            int NEW = MAX * 2;
                            int * tmp = static_cast < int * > ( realloc
                                ( RC, NEW * sizeof RC [ 0 ] ) );
                            if ( tmp != NULL ) {
                                RC = tmp;
                                for ( int i = ALLOCATED; i < NEW; ++ i )
                                    RC [ i ] = 0;
                            }
                            else {
                                rc = RC ( rcExe, rcStorage,
                                    rcAllocating, rcMemory, rcExhausted );
                                cerr << "Out of memory" << endl;
                                break;
                            }
                            ALLOCATED = NEW;
                        }
                    }
                    ++ RC [ depth ];
                    ++ NUM;
                }
            }

            slice_start += slice_len;
        }

        rc = RunCoverage ( pe, & refname, slice_start, 1, coverage );
        if ( rc != SILENT_RC
            ( rcAlign, rcQuery, rcAccessing, rcItem, rcInvalid ) )
        {
            cerr << "Unexpected rc=" << rc << " while calling RunCoverage"
                  "(" << accession << ", " << name << ", "
                      << slice_start << ", 1)" << endl;
            if ( rc == 0 )
                rc = 1;
        }
        else
            rc = 0;

        if ( rc == 0 ) {
            int64_t q [ 5 ];
            for ( unsigned i = 0; i < sizeof q / sizeof q [ 0 ]; ++ i )
                q [ i ] = -1;
            for ( unsigned i = 0, c = 0; i < MAX; ++ i ) {
                int depth = i + 1;
                c += RC [ i ];
                if ( q [ 0 ] == -1 && .10 * NUM < c )
                     q [ 0 ] = depth;
                if ( q [ 1 ] == -1 && .25 * NUM < c )
                     q [ 1 ] = depth;
                if ( q [ 2 ] == -1 && .50 * NUM < c )
                     q [ 2 ] = depth;
                if ( q [ 3 ] == -1 && .75 * NUM < c )
                     q [ 3 ] = depth;
                if ( q [ 4 ] == -1 && .90 * NUM < c ) {
                     q [ 4 ] = depth;
                     break;
                }
            }

            cout << name;
            for ( unsigned i = 0; i < sizeof q / sizeof q [ 0 ]; ++ i ) {
                if ( q [ i ] == -1 )
                    break;
                cout <<  "\t" << q [ i ];
            }
            if ( TESTING ) {
                cout <<  "\t" << len << "|" << NUM << "|" << MAX;
                for ( unsigned i = 0; i < MAX; ++ i )
                    cout <<  "|" << RC [ i ];
            }
            cout << endl;

            memset ( RC, 0, MAX * sizeof RC [ 0 ] );
        }
    }

    free ( RC );
    RC = 0;

    RELEASE ( PileupEstimator, pe );
    RELEASE ( ReferenceList  , rl );
    RELEASE ( VDBManager    , mgr );

    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);
    return run ( argc, app.getArgV() );
}
