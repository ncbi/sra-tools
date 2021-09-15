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
import ngs.ReferenceIterator;


public class DumpReferenceFASTA
{
    private static void process ( Reference ref )
        throws ErrorMsg
    {
        long len = ref . getLength ();

        int line = 0;

        System.out.println ( ">" + ref . getCanonicalName () );
        try
        {
            for ( long offset = 0; offset < len; offset += 5000 )
            {
                String chunk = ref . getReferenceChunk ( offset, 5000 );
                int chunk_len = chunk . length ();
                for ( int chunk_idx = 0; chunk_idx < chunk_len; )
                {
                    int endIndex = chunk_idx + 70 - line;
                    if ( endIndex > chunk_len )
                        endIndex = chunk_len;
                    String chunk_line
                        = chunk . substring ( chunk_idx, endIndex );
                    line += chunk_line . length ();
                    chunk_idx += chunk_line . length ();

                    System.out.print ( chunk_line );
                    if ( line >= 70 )
                    {
                        System.out.println ();
                        line = 0;
                    }
                }
            }
        }
        catch ( ErrorMsg x )
        {
        }
    }


    private static void run ( ReadCollection run, String reference )
        throws ErrorMsg
    {
        Reference ref = run . getReference ( reference );
        process ( ref );
    }


    private static void run ( ReadCollection run )
        throws ErrorMsg
    {
        ReferenceIterator refs = run . getReferences ();
        while ( refs . nextReference () )
        {
            process ( refs );
            System.out.println();
        }
    }

    public static void main ( String [] args )
    {
        boolean failure = true;

        if ( args.length < 1 || args.length > 2 )
        {
            System.out.print
                ( "Usage: DumpReferenceFASTA accession [ reference ]\n" );
        }
        else try
        {
            String acc = args[0];
            // open requested accession using SRA implementation of the API
            ReadCollection run
                = gov.nih.nlm.ncbi.ngs.NGS.openReadCollection ( acc );
            if ( args.length == 2 )
                run ( run, args[1] );
            else
                run ( run );
            failure = false;
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

        if ( failure)
            System.exit ( 10 );
    }
}
