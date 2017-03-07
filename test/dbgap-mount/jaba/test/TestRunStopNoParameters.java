package jaba.test;

import jaba.App;
import jaba.platform.Platform;
import jaba.DbGapMountTestEnv;
import java.io.BufferedReader;

/*>>> Simple test. Should start mounter without parameters.
 <<<  Program should print help and return non-zero code
  >>>
 <<<*/

public 
class TestRunStopNoParameters
    extends TestSuper {

public
TestRunStopNoParameters ( Platform platform )
{
    super ( platform );
}   /* TestRunStopNoParameters () */

public
boolean
gentleTerminate ()
{
        /* Nothing to do
         */
    return false;
}   /* gentleTerminate () */

protected
int
runTest ()
{
    _env.createTestEnv ( DbGapMountTestEnv.EnvType.NoConf );

    System.out.format ( "[Starting application without parameters]\n" );

    App app = new App (
                _platform.getTarget ( Platform.TargetName.MountApp ),
                _platform.getProcessEnvironment (),
                this
                );

    app.start ( 5 );    /* I think that 5 seconds is OK */
    app.waitFor ();

    try {
        BufferedReader out = new BufferedReader ( app.getOutStream () );
        boolean isUsage = false;

        String line;
        while ( ( line = out.readLine() ) != null ) {
            System.out.println(line);

            if ( line.indexOf ( "Usage" ) != 1 * - 1 ) {
                isUsage = true;
            }
        }

        out.close ();

        if ( isUsage ) {
            System.out.format ( "[Confirmed usage output]\n" );
        }
        else {
            System.out.format ( "[ERROR: output does not contains usage information]\n" );
            throw new RuntimeException ( "Application output does not contains usage information" );
        }
    }
    catch ( Throwable Th ) {
        System.out.println ( Th.toString () );

        return 1;
    }

    int retVal = app.exitStatus ();

    System.out.format ( "[Application exited with RC = %d]\n", retVal );

    _env.clearTestEnv ();

    return retVal == 0 ? 1 : 0;
}

};
