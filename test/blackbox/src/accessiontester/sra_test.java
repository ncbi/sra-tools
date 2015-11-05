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
import java.util.ArrayList;
import java.util.List;

public class sra_test
{
    private static final String EXTENSION = ".TEST";
    
    public final String tool;
    public final String args;
    public final String name;
    public final int timeout;
    public boolean for_database;
            
    public TestResult run( final test_runner runner, final sra_type test_object, int pause )
    {
        boolean can_run = true;
        StringBuilder sb = new StringBuilder();
        List< String > cmd_list = new ArrayList<>();

        String id = String.format( "#%d %s.%s ",
                    test_object.number, test_object.acc, name );
        
        cmd_list.add( String.format( "%s%s", runner.get_toolpath(), tool ) );
        
        sb.append( id );
        sb.append( tool );
        
        String a[] = args.split( " " );
        for ( String arg : a )
        {
            switch ( arg )
            {
                case "{acc}" : if ( can_run )
                                {
                                    can_run = !test_object.acc.isEmpty();
                                    if ( can_run )
                                    {
                                        cmd_list.add( test_object.acc );
                                        sb.append( " " );
                                        sb.append( test_object.acc );
                                    }
                                } break;
                    
                case "{slice}" : if ( can_run )
                                {
                                    can_run = !test_object.slice.isEmpty();
                                    if ( can_run )
                                    {
                                        cmd_list.add( test_object.slice );
                                        sb.append( " \"" );
                                        sb.append( test_object.slice );
                                        sb.append( "\"" );
                                    }
                                } break;
                    
                default :   cmd_list.add( arg );
                            sb.append( " " );
                            sb.append( arg );
                            break;
            }
        }
        
        TestResult res;
        if ( can_run )
        {
            res = runner.run( cmd_list, timeout, pause );
            if ( res.ret == 0 )
            {
                sb.append( String.format( " t=%d msec, %d bytes", res.exec_time, res.output_bytes ) );
            }
            else
            {
                switch( res.type )
                {
                    case TIMEOUT : sb.append( " timeout" ); break;
                    case ERROR   : sb.append( " error" ); break;
                    case EMPTY   : sb.append( " empty" ); break;    
                }
            }
            runner.log( sb.toString() );
        }
        else
        {
            res = new TestResult();
            res.type = TestResultType.SKIPPED;
        }
        res.acc = test_object.acc;
        res.test = name;
        res.command = sb.toString();
        res.id = id;
        return res;
    }
    
    public sra_test( final File src_file )
    {
        name = src_file.getName().replaceFirst( EXTENSION, "" ).toUpperCase();
        IniFile ini = new IniFile( src_file.getAbsolutePath(), "sra-test" );
        if ( ini.is_valid() )
        {
            tool = ini.get_str( "tool" );
            for_database = ini.get_bool( "database" );
            args = ini.get_str( "args" );
            timeout = ini.get_int( "timeout" );
        }
        else
        {
            tool = "";
            for_database = false;            
            args = "";
            timeout = 0;
        }
    }
}
