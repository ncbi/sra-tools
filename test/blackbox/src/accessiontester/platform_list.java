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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class platform_list
{
    private static final String EXTENSION = ".PLATFORM";
    public final Map< String , List<String> > platforms;

    private int load( File f, List< String > l, final CommonLogger logger )
    {
        int res = 0;
        try( BufferedReader br = new BufferedReader( new FileReader( f ) ) )
        {
            for ( String line; ( line = br.readLine() ) != null; )
            {
                if ( !line.isEmpty() && !line.startsWith( "#" ) )
                {
                    l.add( line );
                    res++;
                }
            }
        }
        catch ( IOException ex )
        {
            logger.log( String.format( "platform_list: %s", ex.toString() ) );
        }
        return res;
    }
    
    public platform_list( final String directory, final CommonLogger logger )
    {
        platforms = new HashMap<>();
        File folder = new File( directory );
        File[] files = folder.listFiles( ( File dir, String name )
                -> name.toUpperCase().endsWith( EXTENSION ) );
        for ( File item : files )
        {
            List< String > l = new ArrayList<>();
            if ( load( item, l, logger ) > 0 )
            {
                String key = item.getName().replaceFirst( EXTENSION, "" ).toUpperCase();
                platforms.put( key, l );
            }
        }
    }
}
