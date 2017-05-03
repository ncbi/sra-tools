package jaba.test;

import jaba.App;
import jaba.platform.Platform;
import jaba.DbGapMountTestEnv;
import java.io.BufferedReader;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.Path;

/*>>> Simple test. Should start mounter without parameters.
 <<<  Program should print help and return non-zero code
  >>>
 <<<*/

public 
class TestFunctionality
    extends TestSuper {

public
TestFunctionality ( Platform platform )
{
    super ( platform );
}   /* TestFunctionality () */

protected
int
runTest ()
{
    _env.createTestEnv ( DbGapMountTestEnv.EnvType.GoodConf );

    System.out.format ( "[Starting application good config]\n" );

    String Cmd = _platform.getTarget ( Platform.TargetName.MountApp );

    Cmd += " " + _platform.getTarget ( Platform.TargetName.Project );
    Cmd += " " + _platform.getTarget ( Platform.TargetName.MountPoint );

    App app = new App ( Cmd, _platform.getProcessEnvironment (), this );

    app.start ( 5 );

        /*  We need that delay to allow system to wake up
         */
    sleepSec ( 2 );

    simpleMountCheck ( );

    if ( umount () != 0 ) {
        throw new RuntimeException ( "Can umount [" + _platform.getTarget ( Platform.TargetName.MountPoint ) + "]" );
    }

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
        String line;
        while ( ( line = out.readLine() ) != null ) {
            System.out.println ( line );
        }
        out.close ();

        System.out.println ( "[ERR: End]" );
    }
    catch ( Throwable Th ) {
        System.out.println ( Th.toString () );
        return 1;
    }

    int retVal = app.exitStatus ();
    System.out.format ( "[Application exited with RC = %d]\n", retVal );

    _env.clearTestEnv ();

    return retVal;
}

};
