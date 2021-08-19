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

#include <ncbi-vdb/NGS.hpp>
#include <ngs/ErrorMsg.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/Reference.hpp>
#include <ngs/Alignment.hpp>
#include <ngs/PileupIterator.hpp>

#include <math.h>
#include <stdio.h>
#include <iostream>

using namespace ngs;
using namespace std;

class PileupTest
{
public:

    static void run ( String acc, String refname, int start, int stop )
    {

        // open requested accession using SRA implementation of the API
        ReadCollection run = ncbi::NGS::openReadCollection ( acc );
        String run_name = run.getName ();

        // get requested reference
        Reference ref = run.getReference ( refname );

        // start iterator on requested range
        long count = stop - start + 1;
        PileupIterator it = ref.getPileupSlice ( start-1 /*0-based*/, count);

        long i;
        for ( i = 0; it . nextPileup (); ++ i )
        {
            String qual;
            String base;
            cout         << it.getReferenceSpec ()
                 << '\t' << ( it.getReferencePosition () + 1 )
                 << '\t' << it.getReferenceBase () 
                 << '\t' << it.getPileupDepth ();
            while(it.nextPileupEvent())
            {
                PileupEvent::PileupEventType e = it.getEventType ();

                //cout << e <<'\n';

                if(e & PileupEvent::alignment_start)
                {
                    base += '^';
                    base += (char) (it.getMappingQuality() + 33 );
                }
                if(e & PileupEvent::insertion)
                {
                    base += '+';
                    StringRef ibases= it.getInsertionBases();
                    int c = ibases.size();
                    char buf[64];
                    if(e & PileupEvent::alignment_minus_strand)
                    {
                        char *b = buf + sprintf(buf,"%d",c);
                        const char *s = ibases.data();
                        for(int i=0; i<c;i++,b++,s++)
                        {
                            *b=tolower(*s);
                        }
                        *b='\0';
                    }
                    else 
                        sprintf(buf,"%d%.*s",c,c,ibases.data());
                    base += buf;
                }
                if ( ( e & PileupEvent::alignment_minus_strand ) != 0 )
                {
                    switch ( e & 7 )
                    {
                    case PileupEvent::deletion:
                        base += '<';
                        break;
                    case PileupEvent::match:
                        base += ',';
                        break;
                    case PileupEvent::mismatch:
                        base += tolower(it.getAlignmentBase ());
                        break;
                    }
                }
                else
                {
                    switch ( e & 7 )
                    {
                    case PileupEvent::deletion:
                        base += '>';
                        break;
                    case PileupEvent::match:
                        base += '.';
                        break;
                    case PileupEvent::mismatch:
                        base += toupper(it.getAlignmentBase ());
                        break;
                    }
                }
                if(e & PileupEvent::alignment_stop)
                {
                    base += '$';
                }

                qual += it.getAlignmentQuality ();
            }
            cout << '\t' + base
                 << '\t' + qual
                 << '\n';
        }

        cerr << "Read " <<  i <<  " pileups for " <<  run_name << '\n';
    }
};

int main ( int argc, char const *argv[] )
{
    if ( argc != 5 )
    {
        cerr << "Usage: PileupTest accession reference start stop\n";
    }
    else try
    {
        ncbi::NGS::setAppVersionString ( "PileupTest.1.1.0" );
        PileupTest::run ( argv[1], argv[2], atoi ( argv[3] ), atoi ( argv[4] ) );
        return 0;
    }
    catch ( ErrorMsg & x )
    {
        cerr <<  x.toString () << '\n';
    }
    catch ( exception & x )
    {
        cerr <<  x.what () << '\n';
    }
    catch ( ... )
    {
        cerr <<  "unknown exception\n";
    }

    return 10;
}
