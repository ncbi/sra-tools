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

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

public class perform_all_tests
{
    private final tester_params tp;
    private int success, errors, skipped, timeouts, empty, md5_errors, md5_inits;
    private long exec_time;
    private final IniFile md5_sums;
    private final type_list l_types;
    private final platform_list l_platforms;
    private final test_list l_tests;
    
    private List< TestResult > collect_results( List< Future< TestResult > > future_results )
    {
        List< TestResult > results = new ArrayList<>();
        for ( Future< TestResult > future : future_results )
        {
            try
            {
                TestResult res = future.get();
                switch( res.type )
                {
                    case SUCCESS : success++; exec_time += res.exec_time; break;
                    case ERROR   : errors++; break;
                    case TIMEOUT : timeouts++; break;
                    case EMPTY   : empty++; break;
                    case SKIPPED : skipped++; break;
                }
                results.add( res );
            }
            catch ( InterruptedException | ExecutionException ex )
            {
                tp.logger.log( String.format( "collect_results: %s",  ex.toString() ) );
                TestResult res = new TestResult();
                res.ret = -1;
                res.type = TestResultType.TIMEOUT;
                timeouts++;
            }
        }
        if ( success > 0 ) exec_time /= success;
        return results;
    }
    
    private TestWorker make_worker( final test_list l_tests,
                                    final sra_type t,
                                    final String testname )
    {
        TestWorker res = null;
        sra_test st = l_tests.tests.get( testname.toUpperCase() );
        if ( st != null )
        {
            if ( t.is_db == st.for_database )
                res = new TestWorker( t, st, tp );
        }
        else
            tp.logger.log( String.format( "TEST: '%s' not found (#%d: %s)",
                    testname, t.number, t.acc ) );
        return res;
    }

    private boolean list_contains_string( final List< String > l, final String s )
    {
        final String s2 = s.toUpperCase();
        for ( String s1 : l )
        {
            if ( s1.toUpperCase().compareTo( s2 ) == 0 )
                return true;
        }
        return false;
    }
    
    private void add_workers_for_sra_type( final platform_list l_platforms,
                                           final test_list l_tests,
                                           final List< TestWorker > worker_list,
                                           final sra_type t )
    {
        final String platformname = t.platform.toUpperCase();
        List< String > l = new ArrayList<>();
        
        List< String > l_cmn = l_platforms.platforms.get( "COMMON" );
        List< String > l_pf  = l_platforms.platforms.get( platformname );
        if ( l_cmn != null )
        {
            for ( String s : l_cmn ) l.add( s );
        }
        if ( l_pf != null )
        {
            for ( String s : l_pf )
            {
                if ( !list_contains_string( l, s ) ) l.add( s );
            }
        }
        
        if ( tp.cmdline_tests != null && tp.cmdline_tests.length > 0 )
        {
            for ( String testname : tp.cmdline_tests )
            {
                TestWorker worker = make_worker( l_tests, t, testname );
                if ( worker != null ) worker_list.add( worker );
            }
        }
        else
        {
            for ( String testname : l )
            {
                TestWorker worker = make_worker( l_tests, t, testname );
                if ( worker != null ) worker_list.add( worker );
            }
        }
    }
            
    public List< TestResult > perform()
    {
        final List< TestWorker > worker_list = new ArrayList<>();
        
        if ( tp.cmdline_accessions != null && tp.cmdline_accessions.length > 0 )
        {
            for ( String acc : tp.cmdline_accessions )
            {
                sra_type t = l_types.find_entry( acc );
                if ( t != null )
                    add_workers_for_sra_type( l_platforms, l_tests, worker_list, t );    
            }
        }
        else
        {
            for ( sra_type t : l_types.types )
            {
                add_workers_for_sra_type( l_platforms, l_tests, worker_list, t );
            }
        }

        if ( !tp.ordered )
            Collections.shuffle( worker_list );
        
        final ExecutorService executor = Executors.newFixedThreadPool( tp.num_threads );
        final List< Future< TestResult > > future_results = new ArrayList<>();
        
        for ( TestWorker worker : worker_list )
        {
                future_results.add( executor.submit( worker ) );
        }
        
        List< TestResult > results = collect_results( future_results );
        executor.shutdown();
        return results;
    }

    private void md5_check( TestResult res, boolean force )
    {
        String key = String.format( "%s.%s", res.acc, res.test );
        String expected_md5 = md5_sums.get_str( key );
        if ( expected_md5.isEmpty() )
        {
            md5_sums.set_str( key, res.md5 );
            md5_inits++;
        }
        else
        {
            if ( !expected_md5.equals( res.md5 ) )
            {
                if ( force )
                {
                    md5_sums.set_str( key, res.md5 );
                    md5_inits++;
                }
                else
                {
                    md5_errors++;
                    tp.logger.log( String.format( "md5-error: %s (%s vs %s)",
                            res.command, expected_md5, res.md5 ) );
                }
            }
        }
    }
    
    public int evaluate( List< TestResult > results, boolean force )
    {
        tp.logger.log( String.format( "success : %d, errors: %d, timeouts : %d, empty : %d, skipped : %d, avg. time : %d ms",
                success, errors, timeouts, empty, skipped, exec_time ) );
        
        for ( TestResult res : results )
        {
            switch( res.type )
            {
                case SUCCESS : md5_check( res, force ); break;
                case ERROR   : tp.logger.log( String.format( "error  : %s", res.id ) ); break;
                case TIMEOUT : tp.logger.log( String.format( "timeout: %s", res.id ) ); break;
                case EMPTY   : tp.logger.log( String.format( "empty: %s", res.id ) ); break;
                case SKIPPED : break;
            }
        }
        
        if ( md5_inits > 0 ) md5_sums.store();
        tp.logger.log( String.format( "md5 %d errors, %d new md5-sums", md5_errors, md5_inits ) ); 
        
        return errors + timeouts + empty + md5_errors;
    }
    
    public perform_all_tests( final tester_params tp )
    {
        this.tp = tp;
        success = 0; errors = 0; skipped =0; timeouts = 0; empty = 0;
        md5_errors = 0; md5_inits = 0;
        exec_time = 0;
        md5_sums = new IniFile( tp.md5_file, "test-results" );
        l_types = new type_list( tp.input_file, tp.logger );
        l_platforms = new platform_list( tp.test_file_path, tp.logger );
        l_tests = new test_list( tp.test_file_path );
    }
}
