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
import ngs.Reference;
import ngs.Alignment;
import ngs.Pileup;
import ngs.PileupIterator;
import ngs.PileupEvent;
import ngs.PileupEventIterator;

public class PileupTest
{
    static void run ( String acc, String refname, int start, int stop )
        throws ErrorMsg, Exception
    {

        // open requested accession using SRA implementation of the API
        ReadCollection run = gov.nih.nlm.ncbi.ngs.NGS.openReadCollection ( acc );
        String run_name = run.getName ();

        // get requested reference
        Reference ref = run.getReference ( refname );

        // start iterator on requested range of primary alignments
        long count = stop - start + 1;
        PileupIterator it = ref.getPileupSlice ( start - 1, count );

        // walk all primary alignments within range
        long i;
        for ( i = 0; it.nextPileup (); ++ i )
        {
            String qual = "";
            String base = "";

            System.out.print (        it.getReferenceSpec ()
                               +'\t'+ ( it.getReferencePosition () + 1 )
                               +'\t'+ it.getReferenceBase ()
                               +'\t'+ it.getPileupDepth ()
                );

            while ( it . nextPileupEvent () )
            {
                int e = it.getEventType ();
                if ( ( e & PileupEvent.alignment_start ) != 0 )
                {
                    base += '^';
                    base += ( char ) ( it.getMappingQuality () + 33 );
                }

                if ( ( e & PileupEvent.insertion ) != 0 )
                {
                    base += '+';
                    String ibases= it.getInsertionBases();
                    int c = ibases.length();
                    base += Integer.toString(c);

                    if( (e & PileupEvent.alignment_minus_strand ) != 0)
                        base += ibases.toLowerCase ();
                    else
                        base += ibases;
                }

                int evt = e & 7;

                if ( ( e & PileupEvent.alignment_minus_strand ) != 0 )
                {
                    if ( evt == PileupEvent.deletion )
                        base += '<';
                    else if ( evt == PileupEvent.match )
                        base += ',';
                    else if ( evt == PileupEvent.mismatch )
                        base += Character.toLowerCase ( it.getAlignmentBase () );
                }
                else
                {
                    if ( evt == PileupEvent.deletion )
                        base += '>';
                    else if ( evt == PileupEvent.match )
                        base += '.';
                    else if ( evt == PileupEvent.mismatch )
                        base += Character.toUpperCase (it.getAlignmentBase () );
                }
                
                if ( ( e & PileupEvent.alignment_stop ) != 0 )
                    base +='$';
                
                qual += it . getAlignmentQuality ();
            }

            System.out.println (   '\t' + base
                                 + '\t' + qual
                );
        }

        // report to stderr
        System.err.println ( "Read " + i + " pileups for " + run_name );

    }

    public static void main ( String [] args )
    {
        if ( args.length != 4 )
        {
            System.out.print ( "Usage: PileupTest accession reference start stop\n" );
        }
        else try
        {
            run ( args[0], args[1], Integer.parseInt ( args[2] ), Integer.parseInt ( args[3] ) );
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
