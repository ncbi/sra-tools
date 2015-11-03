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

import java.util.List;

public class AccessionTester
{
    private static final String dflt_threads = "6";
    private static final String dflt_tests = "tests";
    private static final String dflt_acc = "acc.txt";
    private static final String dflt_md5 = "md5.txt";
    private static final String dflt_log = ".";
    
    private static void help()
    {
        System.out.println( "accession tester: " );
        System.out.println( "--tools   -t : use the binary tools at this path (no default!)" );        
        System.out.println( String.format( "--acc     -a : file containing the accession to test (dflt: '%s')", dflt_acc ) );
        System.out.println( String.format( "--tests   -e : path to the test-files (dflt: './%s')", dflt_tests ) );
        System.out.println( String.format( "--md5     -m : file containing the md5-sums (dflt: './%s')", dflt_md5 ) );
        System.out.println( String.format( "--threads -n : use this many threads (dflt: %s)", dflt_threads ) );
        System.out.println( String.format( "--log     -l : log file path (dflt: '%s')", dflt_log ) );
        System.out.println( "--run     -u : run only these tests" );
        System.out.println( "--force   -f : overwrite md5-sum in md5-file if different" );
        System.out.println( "--ordered -o : process accessions in the order found in the file" );
        System.out.println( "--help    -h : print this help" );
    }
    
    private static Commandline parse_commandline( final String[] args )
    {
        Commandline cl = new Commandline();
        cl.add_value_rule( null,         "--tools",   "-t" );
        cl.add_value_rule( dflt_acc,     "--acc",     "-a" );
        cl.add_value_rule( dflt_tests,   "--tests",   "-e" );
        cl.add_value_rule( dflt_md5,     "--md5",     "-m" );
        cl.add_value_rule( dflt_log,     "--log",     "-l" );
        cl.add_value_rule( null,         "--run",     "-u" );
        cl.add_value_rule( dflt_threads, "--threads", "-n" );
        cl.add_flag_rule( "--force",   "-f" );
        cl.add_flag_rule( "--ordered", "-o" );
        cl.add_flag_rule( "--help",    "-h" );
        cl.parse( args );
        return cl;
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main( String[] args )
    {
        Commandline cl = parse_commandline( args );

        boolean help_requested = cl.get_option_count( "-h" ) > 0;
        if ( help_requested )
        {
            help();
            System.exit( 0 );
        }

        tester_params tp =  new tester_params( cl );
        if ( !tp.valid )
            System.exit( -1 );
        
        tp.logger.log( "*****************************" );
        tp.logger.log( "     START" );
        tp.logger.log( "*****************************" );

        long startTime = System.currentTimeMillis();
        
        perform_all_tests performer = new perform_all_tests( tp );
        List< TestResult > results = performer.perform();

        boolean force_requested = cl.get_option_count( "-f" ) > 0;
        int res = performer.evaluate( results, force_requested );

        long seconds = ( System.currentTimeMillis() - startTime ) / 1000;
        
        tp.logger.stop( String.format( "done result: %d in ( %d seconds )",
                res, seconds ) );

        System.exit( res );
    }
  
}
