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

#include <NGS.hpp>
//#include <ngs-bam/ngs-bam.hpp>
#include <ngs/ErrorMsg.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/AlignmentIterator.hpp>
#include <ngs/Alignment.hpp>

#include <math.h>
#include <iostream>

using namespace ngs;
using namespace std;

class AlignTest
{
public:

    static AlignmentIterator get_iterator ( ReadCollection & run, int splitNum, int splitNo )
    {
        if ( splitNum <= 1 )
            return run.getAlignments ( Alignment::primaryAlignment );

        // compute window to iterate through
        long MAX_ROW = run.getAlignmentCount ();
        double chunk = ( double ) MAX_ROW / splitNum;
        long first = ( long ) round ( chunk * ( splitNo-1 ) );

        long next_first = ( long ) round ( chunk * ( splitNo ) );
        if ( next_first > MAX_ROW )
            next_first = MAX_ROW;

        //start iterator on reads
        long count = next_first - first;
        return run.getAlignmentRange ( first+1, count, Alignment::primaryAlignment );
    }

    static void run_common ( ReadCollection & run, int splitNum, int splitNo )
    {
        String run_name = run.getName ();

        //start iterator on reads
        AlignmentIterator it = get_iterator ( run, splitNum, splitNo );

        long i;
        for ( i = 0; it.nextAlignment (); ++ i )
        {
            cout         << it.getReadId ()
                 << '\t' << it.getReferenceSpec ()
                 << '\t' << it.getAlignmentPosition ()
                 << '\t' << it.getShortCigar ( false )  // unclipped
                 << '\t' << it.getFragmentBases ()
                 << '\n';
        }

        cerr << "Read " <<  i <<  " alignments for " <<  run_name << '\n';
    }

    static void run_csra ( String acc, int splitNum, int splitNo )
    {
        // open requested accession using SRA implementation of the API
        ReadCollection run = ncbi::NGS::openReadCollection ( acc );
        run_common ( run, splitNum, splitNo );
    }

    static void run_bam ( String acc, int splitNum, int splitNo )
    {
        // open requested accession using example BAM implementation of the API
        //ReadCollection run = NGS_BAM::openReadCollection ( acc );
        //run_common ( run, splitNum, splitNo );
    }

    static void run ( String acc, int splitNum, int splitNo )
    {
        size_t dot = acc . find_last_of ( '.' );
        if ( dot != string :: npos )
        {
            String extension = acc . substr ( dot );
            if ( extension == ".bam" || extension == ".BAM" )
            {
                //run_bam ( acc, splitNum, splitNo );
                return;
            }
        }

        run_csra ( acc, splitNum, splitNo );
    }
};

int main ( int argc, char const *argv[] )
{
    if ( argc != 4 )
    {
        cerr << "Usage: AlignTest accession NumChunks ChunkNo\n";
    }
    else try
    {
        ncbi::NGS::setAppVersionString ( "AlignTest.1.1.0" );
        AlignTest::run (argv[1], atoi ( argv[2] ), atoi ( argv[3] ) );
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
