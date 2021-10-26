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
#include <ngs-bam/ngs-bam.hpp>
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
    static AlignmentIterator getIterator(ReadCollection &collection, int splitNum, int splitNo) {
        if (splitNum > 1) {
            long const MAX_ROW = collection.getAlignmentCount();
            long const chunk = ceil((double)MAX_ROW / splitNum);
            long const first = chunk * (splitNo - 1);
            long const count = (first + chunk <= MAX_ROW) ? chunk : (MAX_ROW - first);

            return collection.getAlignmentRange(first + 1, count, Alignment::primaryAlignment);
        }
        return collection.getAlignments(Alignment::primaryAlignment);
    }
    static void run(ReadCollection &collection, int splitNum, int splitNo)
    {
        long count = 0;
        string const run_name = collection.getName();
        AlignmentIterator it = getIterator(collection, splitNum, splitNo);
        
        while (it.nextAlignment()) {
            ++count;
#if 1
            cout         << it.getReadId ()
                 << '\t' << it.getReferenceSpec ()
                 << '\t' << it.getAlignmentPosition ()
                 << '\t' << it.getShortCigar ( false )  // unclipped
                 << '\t' << it.getFragmentBases ()
                 << '\n';
#endif
        }
        
        cerr << "Read " <<  count <<  " alignments for " <<  run_name << '\n';
    }
    static ReadCollection openReadCollection(String const &name) {
        size_t const length = name.length();
        
        if (length > 4) {
            string const ext = name.substr(length - 4);
            
            if (ext == ".bam") {
                return NGS_BAM::openReadCollection(name);
            }
        }
        return ncbi::NGS::openReadCollection(name);
    }
    static void run ( String acc, int splitNum, int splitNo )
    {
        ReadCollection collection = openReadCollection(acc);

        run(collection, splitNum, splitNo);
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
