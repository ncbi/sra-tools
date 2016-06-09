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
class TestRunStopDaemon
    extends TestSuper {

public
TestRunStopDaemon ( Platform platform )
{
    super ( platform );
}   /* TestRunStopDaemon () */

public
String []
tickets ()
{
    String [] r = { "VDB-1528", "VDB-2841" };
    return r;
}   /* tickets () */

protected
int
runTest ()
{
    _env.createTestEnv ( DbGapMountTestEnv.EnvType.GoodConf );

    System.out.format ( "[Starting application in daemon mode]\n" );

    String Cmd = _platform.getTarget ( Platform.TargetName.MountApp );

    Cmd += " -d";
    Cmd += " " + _platform.getTarget ( Platform.TargetName.Project );
    Cmd += " " + _platform.getTarget ( Platform.TargetName.MountPoint );

    System.out.format ( "[Starting application in daemon mode] [%s]\n", Cmd );

        /*  First we need to start as daemon and wait application exit
         */
    App app = new App ( Cmd, _platform.getProcessEnvironment (), this );

    app.start ( 5 );
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
    if ( retVal != 0 ) {
        System.out.format ( "[Application exited with RC = %d]\n", retVal );
        return 1;
    }
    else {
        System.out.format ( "[Application started in daemon mode]\n" );
    }

        /*  We need that delay to allow system to wake up and check
         *  mount
         */
    sleepSec ( 2 );

    try {
        simpleMountCheck ( );
    }
    catch ( Throwable Th ) {
        System.out.println ( "Mount check failed: " + Th.toString () );
        retVal = 2;
    }

        /*  Umounting
         */
    if ( umount () != 0 ) {
        throw new RuntimeException ( "Can umount [" + _platform.getTarget ( Platform.TargetName.MountPoint ) + "]" );
    }

    _env.clearTestEnv ();

    return retVal;
}

};
