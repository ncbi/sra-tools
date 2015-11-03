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
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.logging.*;

class MyFormatter extends Formatter
{
    @Override public String format( LogRecord rec )
    {
        StringBuilder sb = new StringBuilder();
        sb.append( new java.util.Date() );
        sb.append( ' ' );
        sb.append( formatMessage( rec ) );
        sb.append( '\n' );                        
        return sb.toString();
    }
}

class LogTask implements Runnable
{
    private Logger my_logger;
    private final boolean valid;
    private BlockingQueue<String> log_lines_queue;
    private boolean done;
    
    public void write( final String s )
    {
        if ( valid ) my_logger.log( Level.INFO, s );
    }
    
    @Override public void run()
    {
        try
        {
            do
            {
                write( log_lines_queue.take() );
            } while ( !done );
            
            /* empty the queue after the done flag is set... */
            while ( !log_lines_queue.isEmpty() )
            {
                write( log_lines_queue.poll() );
            }
        }
        catch ( InterruptedException iex )
        {
        }
    }
    
    public void stop()
    {
        done = true;
    }
    
    public LogTask( final String filename, final String info, BlockingQueue<String> q )
    {
        my_logger = null;
        log_lines_queue = q;
        boolean success = false;

        try
        {
            MyFormatter f = new MyFormatter();
                    
            FileHandler my_file_handler = new FileHandler( filename, true );
            my_file_handler.setFormatter( f );
            
            ConsoleHandler my_console_handler = new ConsoleHandler();
            my_console_handler.setFormatter( f );
            
            my_logger = Logger.getLogger( info );
            my_logger.setLevel( Level.INFO );
            my_logger.addHandler( my_console_handler );            
            my_logger.addHandler( my_file_handler );
            
            LogManager my_log_manager = LogManager.getLogManager();
            my_log_manager.addLogger( my_logger );
            
            success = true;
        }   
        catch( IOException io_e )
        {
            System.out.println( "cannot create Log-Filehandler: " + io_e );
        }
        valid = success;
        done = false;
    }
}
        
public class CommonLogger
{
    private final BlockingQueue<String> log_lines_queue;
    private final LogTask my_task;
    private final Thread my_thread;
    
    /**
     * the main functionality: log this string...
     * 
     * @param s ... string to write to log-file ( can be null )
     */
    public void log( final String s )
    {
        if ( s != null )
        {
            try
            {
                log_lines_queue.put( s );
            }
            catch( InterruptedException iex )
            {
                // we simply do nothing then...
            }
        }
    }
    
    /**
     * starts the logging task ( constructor does not! )
     */
    public void start() { my_thread.start(); }
    
    /**
     * stops the logging task
     * 
     * @param s ... optional final string to write
     */
    public void stop( String s )
    {
        if ( s != null )  my_task.write( s );
        my_task.stop();
    }
    
    /**
     *
     * @param filename ... full path to log-file
     * @param info ....... info field used in logging
     */
    public CommonLogger( final String filename, final String info )
    {
        Logger global_logger = Logger.getGlobal();
        Handler[] h = global_logger.getParent().getHandlers();
        for ( int i = 0; i < h.length; ++i )
            global_logger.getParent().removeHandler( h[ i ] );
        
        log_lines_queue = new LinkedBlockingQueue<>();
        my_task = new LogTask( filename, info, log_lines_queue );
        my_thread = new Thread( my_task );
    }
}
