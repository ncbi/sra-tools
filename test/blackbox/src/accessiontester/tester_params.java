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
import java.util.Calendar;
import java.util.Date;

public class tester_params
{
    public final String toolpath;
    public final String test_file_path;
    public final String input_file;
    public final String md5_file;
    public final int num_threads;
    public final int pause;
    public final CommonLogger logger;
    public final boolean valid;
    public final boolean ordered;
    public final String[] cmdline_tests;
    public final String[] cmdline_accessions;
    
    private boolean file_exists( final String filename )
    {
        File f = new File( filename );
        return f.isFile();
    }

    private boolean dir_exists( final String dirname )
    {
        File f = new File( dirname );
        return f.isDirectory();
    }
    
    private boolean check_path( final String path, final String err )
    {
        boolean res = ( path != null );
        if ( res ) res = !path.isEmpty();
        if ( res ) res = dir_exists( path );
        if ( !res ) System.err.println( err );
        return res;
    }

    private boolean check_file( final String path, final String err )
    {
        boolean res = ( path != null );
        if ( res ) res = !path.isEmpty();
        if ( res ) res = file_exists( path );
        if ( !res ) System.err.println( err );
        return res;
    }

    private boolean check_params()
    {
        boolean toolpath_ok = check_path( toolpath, "tool-path invalid!" );
        boolean testpath_ok = check_path( test_file_path, "test-file-path invalid!" );
        boolean input_ok = check_file( input_file, "accession-file invalid!" );

        boolean md5_ok = ( md5_file != null )&&( !md5_file.isEmpty() );
        if ( !md5_ok ) System.err.println( "md5-file invalid!" );
       
        boolean nt_ok = ( num_threads > 0 );
        if ( !nt_ok ) System.err.println( "number of therads invalid!" );
        
        return ( toolpath_ok && testpath_ok && input_ok && md5_ok && nt_ok );
    }
    
    private String get_log_file_name( final String log_file_path )
    {
        String res;
        String file_sep = System.getProperty( "file.separator" );
        Date today = new Date();
        Calendar cal = Calendar.getInstance();
        cal.setTime( today );
        int year  = cal.get( Calendar.YEAR );
        int month = cal.get( Calendar.MONTH ) + 1;
        int day   = cal.get( Calendar.DAY_OF_MONTH );
        
        if ( log_file_path.endsWith( file_sep ) )
            res = String.format( "%s%d_%d_%d.log", log_file_path, year, month, day );
        else
            res = String.format( "%s%s%d_%d_%d.log", log_file_path, file_sep, year, month, day );
        return res;
    }

    private int int_value( final String s )
    {
        return ( s != null && !s.isEmpty() ) ? Integer.valueOf( s ) : 0 ;    
    }
    
    public tester_params( final Commandline cl )
    {
        toolpath        = cl.get_option( "--tools" );
        test_file_path  = cl.get_option( "--tests" );
        input_file      = cl.get_option( "--acc" );
        md5_file        = cl.get_option( "--md5" );
        cmdline_tests   = cl.get_options( "--run" );
        cmdline_accessions = cl.get_arguments();
        num_threads     = int_value( cl.get_option( "--threads" ) );
        pause           = int_value( cl.get_option( "--pause" ) );
        ordered         = ( cl.get_option_count( "--ordered" ) > 0 );
        final String log_file_path = cl.get_option( "--log" );
        if ( log_file_path.compareTo( "-" ) == 0 )
            logger = new CommonLogger( null, "acc-test log" );
        else
        {
            final String log_file_name = get_log_file_name( log_file_path );
            logger = new CommonLogger( log_file_name, "acc-test log" );
        }
        logger.start();

        valid = check_params();
    }
}
