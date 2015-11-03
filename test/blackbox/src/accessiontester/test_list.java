/* ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
=========================================================================== */
package accessiontester;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

public class test_list
{
    private static final String EXTENSION = ".TEST";
    public final Map< String, sra_test > tests;
   
    public test_list( final String directory )
    {
        tests = new HashMap<>();
        File folder = new File( directory );
        File[] files = folder.listFiles( ( File dir, String name )
                -> name.toUpperCase().endsWith( EXTENSION ) );
        for ( File item : files )
        {
            sra_test t = new sra_test( item );
            if ( !t.tool.isEmpty() )
            {
                String key = item.getName().toUpperCase();
                tests.put( key, t );
            }
        }
        
    }
}
