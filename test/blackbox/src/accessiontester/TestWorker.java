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

import java.util.concurrent.Callable;

public class TestWorker implements Callable< TestResult >
{
    private final sra_type test_object;
    private final sra_test test_to_run;
    private final tester_params tp;
    
    @Override public TestResult call()
    {
        test_runner runner = new test_runner( tp );
        return test_to_run.run( runner, test_object, tp.pause );
    }
    
    TestWorker( final sra_type test_object, final sra_test test_to_run,
                final tester_params tp )
    {
        this.test_object = test_object;
        this.test_to_run = test_to_run;
        this.tp = tp;
    }
}
