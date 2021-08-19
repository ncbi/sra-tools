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
#include <ngs/ReadIterator.hpp>
#include <ngs/Read.hpp>


#include <math.h>
#include <iostream>

using namespace ngs;
using namespace std;

class DumpReferenceFASTA
{
public:

    static void process ( const Reference & ref )
    {
        uint64_t len = ref . getLength ();

        size_t line = 0;

        cout << '>' << ref . getCanonicalName () << '\n';

        try
        {
            for ( uint64_t offset = 0; offset < len; offset += 5000 )
            {
                StringRef chunk = ref . getReferenceChunk ( offset, 5000 );
                size_t chunk_len = chunk . size ();
                for ( size_t chunk_idx = 0; chunk_idx < chunk_len; )
                {
                    StringRef chunk_line = chunk . substr ( chunk_idx, 70 - line );
                    line += chunk_line . size ();
                    chunk_idx += chunk_line . size ();

                    cout << chunk_line;
                    if ( line >= 70 )
                    {
                        cout << '\n';
                        line = 0;
                    }
                }
            }
            if (line != 0)
                cout << '\n';
        }
        catch ( ErrorMsg x )
        {
        }
    }

    static void run ( const String & acc, const String & reference )
    {

        // open requested accession using SRA implementation of the API
        ReadCollection run = ncbi::NGS::openReadCollection ( acc );
        Reference ref = run . getReference ( reference );
        process ( ref );
    }

    static void run ( const String & acc )
    {

        // open requested accession using SRA implementation of the API
        ReadCollection run = ncbi::NGS::openReadCollection ( acc );
        ReferenceIterator refs = run . getReferences ();
        while ( refs . nextReference () )
        {
            process ( refs );
        }
    }
};

int main (int argc, char const *argv[])
{
    if ( argc < 2 || argc > 3)
    {
        cerr << "Usage: DumpReferenceFASTA accession [ reference ]\n";
    }
    else try
    {
        ncbi::NGS::setAppVersionString ( "DumpReferenceFASTA.1.0.0" );
        if ( argc == 3 )
            DumpReferenceFASTA::run ( argv[1], argv[2] );
        else
            DumpReferenceFASTA::run ( argv[1] );
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
