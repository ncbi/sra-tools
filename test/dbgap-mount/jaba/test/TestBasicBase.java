package jaba.test;

import jaba.App;
import jaba.Utils;
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

abstract public 
class TestBasicBase
    extends TestSuper {

public
TestBasicBase ( Platform platform )
{
    super ( platform );
}   /* TestBasicBase () */

protected
void
start ( boolean ReadOnly )
{
    if ( ReadOnly ) {
        System.out.format ( "[Starting application in RO mode as daemon]\n" );
    }
    else {
        System.out.format ( "[Starting application in RW mode as daemon]\n" );
    }

    String Cmd = _platform.getTarget ( Platform.TargetName.MountApp );

    Cmd += " -d";
    if ( ReadOnly ) {
        Cmd += " ro ";
    }
    Cmd += " " + _platform.getTarget ( Platform.TargetName.Project );
    Cmd += " " + _platform.getTarget ( Platform.TargetName.MountPoint );

    System.out.format ( "[Starting application in daemon mode] [%s]\n", Cmd );

        /*  First we need to start as daemon and wait application exit
         */
    App app = new App ( Cmd, _platform.getProcessEnvironment (), this );

    app.start ( 5 );
    app.waitFor ();

    Utils.sleepSec ( 2 );

    simpleMountCheck ( );

    System.out.format ( "[Application started as daemon]\n" );
}   /* start () */

protected
void
stop ()
{
    if ( umount () != 0 ) {
        throw new RuntimeException ( "Can umount [" + _platform.getTarget ( Platform.TargetName.MountPoint ) + "]" );
    }
}   /* stop () */

    /* You should reimplement it
     */
abstract protected
void
runActualTest ();

protected
int
runTest ()
{
    try {
        _env.createTestEnv ( DbGapMountTestEnv.EnvType.GoodConf );

        runActualTest ();

        _env.clearTestEnv ();
    }
    catch ( Throwable Th ) {
        System.out.printf ( "Test Failed : %s\n", Th.toString () );
        return 1;
    }

    return 0;
}   /* runTest () */

protected
String
pathToFile ( String fileName )
{
    return    _platform.getTarget ( Platform.TargetName.MountPoint )
            + _platform.getTarget ( Platform.TargetName.Separator )
            + "workspace"
            + _platform.getTarget ( Platform.TargetName.Separator )
            + fileName
            ;
}   /* pathToFile () */

protected
String
pathToTempFile ( String fileName )
{
    return    _platform.getTarget ( Platform.TargetName.TempDir )
            + _platform.getTarget ( Platform.TargetName.Separator )
            + fileName
            ;
}   /* pathToTempFile () */


};
