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

class stream_reader extends Thread
{
    private final InputStream the_stream;
    private final CommonLogger logger;
    private MessageDigest md5;
    private final StringBuilder content;
    public long bytes_received;

    public String get_md5()
    {
        if ( md5 != null && bytes_received > 0 )
        {
            StringBuilder sb = new StringBuilder();
            byte [] b = md5.digest();
            for ( int i = 0; i < b.length; ++i )
                sb.append( Integer.toHexString( ( b[ i ] & 0xFF ) | 0x100 ).substring( 1, 3 ) );
            return sb.toString();
        }
        return "-";
    }
    
    public String get_content() { return content.toString(); }
    
    @Override public void run()
    {
        if ( the_stream != null )
        {
            byte[] buffer = new byte[ 4096 * 8 ];
            int nbytes;
            try
            {
                while ( ( nbytes = the_stream.read( buffer, 0, buffer.length ) ) > 0 )
                {
                    content.append( new String( buffer ) );
                    bytes_received += nbytes;
                    if ( md5 != null )
                        md5.update( buffer, 0, nbytes );
                }
            }
            catch ( IOException ex )
            {
                logger.log( String.format( "test_runner: %s", ex.toString() ) );
            }
        }
    }
            
    public stream_reader( final InputStream a_stream, final CommonLogger a_logger )
    {
        the_stream = a_stream;
        logger = a_logger;
        content = new StringBuilder();
        try
        {
            md5 = MessageDigest.getInstance( "MD5" );
        }
        catch ( NoSuchAlgorithmException ex )
        { 
            logger.log( String.format( "test_runner: %s", ex.toString() ) );
            md5 = null;
        }
        bytes_received = 0;
    }
}

public class test_runner
{
    private final ProcessBuilder pb;
    private final tester_params tp;
    private final String toolpath;
    private final TestResult res;
    
    public void log( final String s ) { tp.logger.log( s ); }
    
    private void join_streams( stream_reader s1, stream_reader s2 )
    {
        try
        {
            s1.join();
            s2.join();
        } catch ( InterruptedException ex )
        {
            tp.logger.log( String.format( "test_runner: %s", ex.toString() ) );
        }
    }
            
    public TestResult run( List <String> list, int timeout, int pause )
    {
        long startTime = System.currentTimeMillis();
        pb.command( list );

        Process proc;
        try { proc = pb.start(); }
        catch ( IOException ex )
        {
            tp.logger.log( String.format( "test_runner: %s", ex.toString() ) );
            proc = null;
            res.set_rettype( TestResultType.ERROR );
        }
        
        if ( proc != null )
        {
            stream_reader stdout_reader = new stream_reader( proc.getInputStream(), tp.logger );
            stdout_reader.start();
            
            stream_reader stderr_reader = new stream_reader( proc.getErrorStream(), tp.logger );            
            stderr_reader.start();
            
            if ( proc.isAlive() )
            {
                try
                {
                    if ( timeout == 0 )
                        res.set_retcode( proc.waitFor() );
                    else
                    {
                        if ( proc.waitFor( timeout, TimeUnit.SECONDS ) )
                            res.set_retcode( proc.exitValue() );
                        else
                            res.set_rettype( TestResultType.TIMEOUT );
                    }
                }
                catch ( InterruptedException ex )
                {
                    tp.logger.log( String.format( "test_runner: %s", ex.toString() ) );
                    res.set_rettype( TestResultType.TIMEOUT );
                }
            }
            else
                res.set_retcode( proc.exitValue() );

            join_streams( stdout_reader, stderr_reader );
            res.output_bytes = stdout_reader.bytes_received + stderr_reader.bytes_received;
            res.md5 = stdout_reader.get_md5();
            
            if ( res.output_bytes == 0 )
                res.set_rettype( TestResultType.EMPTY );
            else
            {
                if ( res.type == TestResultType.ERROR )
                {
                    tp.logger.log( String.format( "ERROR : %d", res.ret ) );
                    tp.logger.log( String.format( "STDOUT: %s", stdout_reader.get_content() ) );
                    tp.logger.log( String.format( "STDERR: %s", stderr_reader.get_content() ) );
                }
            }
        }
        
        if ( pause > 0 )
        {
            try { Thread.sleep( pause );} catch ( InterruptedException ex ) { }
        }
        res.exec_time = ( System.currentTimeMillis() - startTime );
        return res;
    }

    public String get_toolpath()
    {
        return toolpath;
    }
    
    public test_runner( final tester_params tp )
    {
        pb = new ProcessBuilder();
        res = new TestResult();
        this.tp = tp;
        
        String file_sep = System.getProperty( "file.separator" );
        if ( tp.toolpath.endsWith( file_sep ) )
            this.toolpath = tp.toolpath;
        else
            this.toolpath = tp.toolpath + file_sep;
    }
    
}
