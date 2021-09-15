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

package examples;

import ngs.ErrorMsg;
import ngs.ReadCollection;
import ngs.AlignmentIterator;
import ngs.Alignment;

public class AlignTest
{
    static void run ( String acc, int splitNum, int splitNo )
        throws ErrorMsg, Exception
    {

        // open requested accession using SRA implementation of the API
        ReadCollection run = gov.nih.nlm.ncbi.ngs.NGS.openReadCollection ( acc );
        String run_name = run.getName ();

        // compute window to iterate through
        long MAX_ROW = run.getAlignmentCount (); 
        double chunk = ( double ) MAX_ROW / splitNum;
        long first = ( long ) Math.round ( chunk * ( splitNo-1 ) );

        long next_first = ( long ) Math.round ( chunk * ( splitNo ) );
        if ( next_first > MAX_ROW )
            next_first = MAX_ROW;

        // start iterator on primary alignments
        long count = next_first - first;
        AlignmentIterator it = run.getAlignmentRange ( first+1, count, Alignment.primaryAlignment );

        long i;
        for (i = 0; it.nextAlignment (); ++ i )
        {
            System.out.println (        it.getReadId ()
                                 +'\t'+ it.getReferenceSpec ()
                                 +'\t'+ it.getAlignmentPosition()
                                 +'\t'+ it.getShortCigar ( false )  // unclipped
                                 +'\t'+ it.getFragmentBases ()
                  );
        }

        System.err.println ( "Read " + i + " alignments for " + run_name );
    }
    public static void main ( String [] args )
    {
        if ( args.length != 3 )
        {
            System.out.print ( "Usage: AlignTest accession NumChunks ChunkNo\n" );
        }
        else try
        {
            run ( args[0], Integer.parseInt ( args[1] ), Integer.parseInt ( args[2] ) );
        }
        catch ( ErrorMsg x )
        {
            System.err.println ( x.toString () );
            x.printStackTrace ();
        }
        catch ( Exception x )
        {
            System.err.println ( x.toString () );
            x.printStackTrace ();
        }
    }
}
