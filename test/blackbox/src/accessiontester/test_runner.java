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

import java.io.IOException;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;
import java.util.concurrent.TimeUnit;

public class test_runner
{
    private final ProcessBuilder pb;
    private final String toolpath;
    private final TestResult res;
    public final CommonLogger logger;
    
    private String bytes_2_string( byte[] b )
    {
        StringBuilder sb = new StringBuilder();
        for ( int i = 0; i < b.length; ++i )
            sb.append( Integer.toHexString( ( b[ i ] & 0xFF ) | 0x100 ).substring( 1, 3 ) );
        return sb.toString();
    }
    
    public TestResult run( List <String> list, int timeout )
    {
        long startTime = System.currentTimeMillis();
        pb.command( list );

        Process proc;
        try { proc = pb.start(); }
        catch ( IOException ex )
        {
            logger.log( String.format( "test_runner: %s", ex.toString() ) );
            proc = null;
            res.ret = -1;
        }
        
        if ( proc != null )
        {
            InputStream is = proc.getInputStream();

            new Thread()
            {
                @Override public void run()
                {
                    try
                    {
                        MessageDigest md = MessageDigest.getInstance( "MD5" );
                        byte[] buffer = new byte[ 4096 * 32 ];
                        int n_read;
                        while ( ( n_read = is.read( buffer, 0, buffer.length ) ) != -1 )
                        {
                            res.output_bytes += n_read;
                            md.update( buffer, 0, n_read );
                        }
                        res.md5 = bytes_2_string( md.digest() );
                    }
                    catch ( NoSuchAlgorithmException | IOException ex )
                    {
                        logger.log( String.format( "test_runner: %s", ex.toString() ) );
                    }
                }
            }.start();

            if ( proc.isAlive() )
            {
                try
                {
                    if ( timeout == 0 )
                    {
                        res.ret = proc.waitFor();
                        res.type = ( res.ret == 0 ) ? TestResultType.SUCCESS : TestResultType.ERROR;
                    }
                    else
                    {
                        boolean finished = proc.waitFor( timeout, TimeUnit.SECONDS );
                        if ( finished )
                        {
                            res.ret = proc.exitValue();
                            res.type = ( res.ret == 0 ) ? TestResultType.SUCCESS : TestResultType.ERROR;
                        }
                        else
                        {
                            res.type = TestResultType.TIMEOUT;
                            res.ret = -1;
                        }
                    }
                    if ( res.output_bytes == 0 )
                    {
                        res.type = TestResultType.EMPTY;
                        res.ret = -1;
                    }
                }
                catch ( InterruptedException ex )
                {
                    logger.log( String.format( "test_runner: %s", ex.toString() ) );
                    res.ret = -1;
                    res.type = TestResultType.TIMEOUT;
                }
            }
        }
        res.exec_time = ( System.currentTimeMillis() - startTime );
        return res;
    }

    public String get_toolpath()
    {
        return toolpath;
    }
    
    public test_runner( final String toolpath, final CommonLogger logger )
    {
        pb = new ProcessBuilder();
        pb.redirectErrorStream( true );
        res = new TestResult();
        this.logger = logger;
        
        String file_sep = System.getProperty( "file.separator" );
        if ( toolpath.endsWith( file_sep ) )
            this.toolpath = toolpath;
        else
            this.toolpath = toolpath + file_sep;
    }
    
}
