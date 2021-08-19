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


using namespace ngs;
using namespace std;


class RefTest
{
public:

    static void run ( const String &acc )
    {
        // open requested accession using SRA implementation of the API
        ReadCollection run ( ncbi::NGS::openReadCollection ( acc ) );

        String run_name ( run.getName () );

        // get all references
        ReferenceIterator it ( run.getReferences () );

        long i = 0;
        for (i = 0; it.nextReference (); ++ i ) {
            cout        << it.getCommonName ()
                << '\t' << it.getCanonicalName ()
                << '\t' << it.getLength ()
                << '\t' << ( it.getIsCircular () ? "circular" : "linear" )
                << '\n';
        }

        cerr << "\nRead " << i << " references for " + run_name << '\n';
    }
};


int main ( int argc, char const *argv[] )
{
    if ( argc != 2 )
    {
        cerr << "Usage: RefTest accession\n";
        return 10;
    }
    else try
    {
        ncbi::NGS::setAppVersionString ( "RefTest.1.1.0" );
        RefTest::run ( argv[1] );
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
