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
class TestRunStopBadConfig
    extends TestSuper {

public
TestRunStopBadConfig ( Platform platform )
{
    super ( platform );
}   /* TestRunStopBadConfig () */

protected
int
runTest ()
{
    _env.createTestEnv ( DbGapMountTestEnv.EnvType.BadConf );

    System.out.format ( "[Starting application bad config]\n" );

    String Cmd = _platform.getTarget ( Platform.TargetName.MountApp );

    Cmd += " " + _platform.getTarget ( Platform.TargetName.Project );
    Cmd += " " + _platform.getTarget ( Platform.TargetName.MountPoint );

    App app = new App ( Cmd, _platform.getProcessEnvironment (), this );

    app.start ( 5 );    /* I think that 5 seconds is OK */
    app.waitFor ();

    try {
        System.out.println ( "[OUT: Begin]" );

        BufferedReader out = new BufferedReader ( app.getOutStream () );
        String line;
        while ( ( line = out.readLine() ) != null ) {
            System.out.println ( line );
        }
        out.close ();

        System.out.println ( "[OUT: End]" );
    }
    catch ( Throwable Th ) {
        System.out.println ( Th.toString () );
        return 1;
    }

    try {
        System.out.println ( "[ERR: Begin]" );

        BufferedReader out = new BufferedReader ( app.getErrStream () );
        boolean empty = true;
        String line;
        while ( ( line = out.readLine() ) != null ) {
            System.out.println ( line );

            if ( ! line.isEmpty () ) {
                empty = false;
            }
        }
        out.close ();

        System.out.println ( "[ERR: End]" );

        if ( empty ) {
            throw new RuntimeException ( "Missed Error Message body" );
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
