package jaba;

import jaba.platform.Platform;
import jaba.test.Tests;
// import jaba.test.Test;

public
class
DbGapMountTest {

public static
void
main( String args [] )
{
    try {
        if ( args.length != 1 ) {
            System.out.println ( "Invalid arguments" );

            printHelp ();

            throw new RuntimeException ( "Invalid arguments" );
        }

        String ExeDir = args [ 0 ];

        Platform Plt = Platform.getPlatform ( ExeDir );

        Tests tests = new Tests ( Plt );

        tests.run ();
    }
    catch ( Exception Expt ) {
        Expt.printStackTrace ();
    }
}

public static
void
printHelp ()
{
        /* Not sure what is better :LOL: */
    String className = new Object() { }.getClass().getEnclosingClass().getName ();
    // String className = new Object() { }.getClass().getEnclosingClass().getSimpleName ();

    System.out.format (
                        "\nSyntax: java %s bin_directory\n",
                        className
                        );
    System.out.format (
                        "\nWhere:\n\nbin_directory - is directory containing these utilites:\n                dbgap-mount-too, vdb-config, fasq-dump\n\n"
                        );
}   /* printHelp () */

};
