package jaba.test;

import jaba.platform.Platform;
import jaba.DbGapMountTestEnv;
import jaba.App;
import jaba.AppTerminator;
import jaba.Utils;
import java.time.LocalDateTime;
import java.nio.file.*;
import java.io.File;
import java.io.BufferedReader;
import java.util.Vector;
import java.util.StringTokenizer;

public abstract
class
TestSuper
    implements Test, AppTerminator {

protected Platform _platform = null;
protected DbGapMountTestEnv _env = null;
private boolean _running = false; 
private int _exitStatus = 0;

public
TestSuper ( Platform platform )
{
    _platform = platform;

    _env = new DbGapMountTestEnv ( platform );

    _running = false;
    _exitStatus = 0;
}   /* TestSuper () */

public
String
getName ()
{
    return getClass ().getSimpleName ();
}   /* getName () */

    /*>>    By default test should be part of VDB-1528, which is
      ||    umbrella for all other tickets ... or most of them
      <<*/
public  
String []
tickets ()
{
    String [] retVal = { "VDB-1528" };
    return retVal;
}   /* tickets () */

public
int
umount ()
{
    Utils.sleepSec ( 2 );

    String MP = _platform.getTarget ( Platform.TargetName.MountPoint );

    String Cmd = _platform.getTarget ( Platform.TargetName.MountApp );
    Cmd += " -u " + MP;

    System.out.println ( "Unmounting [" + MP + "] [" + Cmd + "]" );

    int RC = 0;

try {
    App app = new App ( Cmd, _platform.getProcessEnvironment () );
    app.start ();
    app.waitFor ();

    RC = app.exitStatus ();
}
catch ( Throwable Th ) {
System.out.println ( "Umount failed : " + Th.toString () );
}

    Utils.sleepSec ( 2 );

    boolean failed = false;

    try {
        simpleUmountCheck ();
    }
    catch ( Throwable Th ) {
        System.out.println ( "Umount check failed : " + Th.toString () );

        failed = true;

        RC = 2;
    }

    return RC;
}   /* umount () */

public
boolean
gentleTerminate ()
{
        /*  I am too lazy to write one more layer, so this would be
         *  common as full, not empty.
         */

        /*  Two step process call "command -u mountpoint"
         *  if does not work, call "dokancnt /m mountpoint" or 
         *                         "fusermount -u mountpoint"
         *  if does not work, return to parent method, destroying
         *  process, but it does never work
         */
    if ( umount () == 0 ) {
        return true;
    }

    String Cmd = _platform.getTarget ( Platform.TargetName.UmountApp );
    Cmd += " " + _platform.getTarget ( Platform.TargetName.MountPoint );

    App app = new App ( Cmd, _platform.getProcessEnvironment () );
    app.start ();
    app.waitFor ();
    if ( app.exitStatus () == 0 ) {
        return true;
    }

    return false;
}   /* gentleTerminate () */

public
void
intro ()
{
    System.out.println ( "" );
    System.out.println ( "||<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" );
    System.out.println ( "||<<< Test STARTED [" + getName () + "] [" + LocalDateTime.now () + "]" );
    System.out.println ( "||<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" );
}   /* intro () */

public
void
outro ()
{
    System.out.println ( "||<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" );
    if ( exitStatus () == 0 ) {
        System.out.println ( "||<<< Test PASSED [" + getName () + "] [" + LocalDateTime.now () + "]" );
    }
    else {
        System.out.println ( "||<<< Test FAILED [" + getName () + "] [" + LocalDateTime.now () + "]" );
    }
    System.out.println ( "||<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" );
}   /* outro () */

abstract protected
int
runTest ();

public
void
run ()
{
    intro ();

    _running = true;

    try {
        _exitStatus = 0;

        _exitStatus = runTest ();
    }
    catch ( Throwable Th ) {
        System.err.format ( "Test [%s] failed because: %s\n", getName (), Th.toString () );
        _exitStatus = 666;
    }

    _running = false;

    outro ();
}   /* run () */

public
int
exitStatus ()
{
    if ( _running ) {
        throw new RuntimeException ( "Can not return exitStatus on running test [" + getName () + "]" );
    }

    return _exitStatus;
}   /* exitStatus () */

protected
int
runMounter ()
{
    return 0;
}   /* runMounter () */

protected
void
simpleMountCheck ( )
{
    System.out.println ( "Performing mount check on  ["
                + _platform.getTarget ( Platform.TargetName.MountPoint )
                + "]" )
                ;


    String [] Targets = { "cache", "kart-files", "karts", "workspace" };

    for ( int i = 0; i < Targets.length; i ++ ) {
        String ThePath =
                  _platform.getTarget ( Platform.TargetName.MountPoint )
                + _platform.getTarget ( Platform.TargetName.Separator )
                + Targets [ i ]
                ;
        if ( !Files.isDirectory ( Paths.get ( ThePath ) ) ) {
            throw new RuntimeException ( "Invalid mount : can not locate [" + Targets [ i ] + "] directory" );

        }

        System.out.println ( "Found [" + ThePath + "] directory" );
    }

}   /* simpleMountCheck () */

protected
void
simpleUmountCheck ( )
{
    String MP = _platform.getTarget ( Platform.TargetName.MountPoint );

    System.out.println ( "Performing umount check on  [" + MP + "]" ) ;

    if ( _platform.needCreateMountPoint() ) {
        File file = new File ( MP );
        if ( file.isDirectory () ) {
            if ( file.list () != null ) {
                if ( file.list().length > 0 ) {
                    throw new RuntimeException ( "Mount point still mounted [" + MP + "]" );
                }
            }
        }
        else {
            throw new RuntimeException ( "Mount point is not a directory [" + MP + "]" );
        }
    }
    else {
        if ( new File ( MP ).exists () ) {
            throw new RuntimeException ( "Mount point still exists [" + MP + "]" );
        }
    }
}   /* simpleUmountCheck () */

protected
void
sleepSec ( long sec )
{
    try { Thread.sleep ( 1000 * sec ); } catch ( Throwable Th ) { };
}   /* sleepSec () */

/* There are IO methods to use in tests. They are not cool, but I
 * had to do that.
 */

private
String []
runWrapCmd ( String params, boolean return_out )
{
    String [] retVal = {};
    String cmd = "java jaba.WrapCmd";

    try {
        cmd += " " + params;

        // System.out.println ( "RUN[" + cmd + "]" );

        App app = new App ( cmd, _platform.getProcessEnvironment () );
        app.start ();
        app.waitFor ();

        int RC = app.exitStatus ();

        if ( return_out ) {
            Vector < String > lines = new Vector < String > ();
            BufferedReader out = new BufferedReader ( app.getOutStream () );
            String line;
            while ( ( line = out.readLine() ) != null ) {
                line = line.trim ();
                if ( ! line.isEmpty () ) {
                    lines.addElement ( line );
                }
            }
            out.close ();

            retVal = lines.toArray ( retVal );
        }
        else {
            BufferedReader out = new BufferedReader ( app.getOutStream () );
            String line;
            boolean was_out = false;
            while ( ( line = out.readLine() ) != null ) {
                if ( ! was_out ) {
                    System.out.println ( "[STDOUT: begin]" );
                }
                System.out.println ( line );
            }
            out.close ();

            if ( was_out ) { System.out.println ( "[STDOUT: end]" ); }
            // else { System.out.println ( "[STDOUT: empty]" ); }
        }

        {
            BufferedReader out = new BufferedReader ( app.getErrStream () );
            String line;
            boolean was_out = false;
            while ( ( line = out.readLine() ) != null ) {
                if ( ! was_out ) {
                    System.out.println ( "[STDERR: begin]" );
                }
                System.out.println ( line );
            }
            out.close ();

            if ( was_out ) { System.out.println ( "[STDERR: end]" ); }
            // else { System.out.println ( "[STDERR: empty]" ); }
        }

    }
    catch ( Throwable Th ) {
        System.out.println ( "Error: " + Th.toString () );

        throw new RuntimeException ( "Error: can not run command [" + cmd + "]" );
    }

    return retVal;
}   /* runWrapCmd () */

protected
void
createFile ( String fileName, int fileSize, boolean deleteAfter )
{
    System.out.printf ( "createFile [%s] with size [%d]\n", fileName, fileSize );
    String params =   "CF "
                    + fileName
                    + " "
                    + new Integer ( fileSize ).toString ()
                    ;
    runWrapCmd ( params, false );

    sleepSec ( 1 );

    int size = fileSize ( fileName );
    if ( size != fileSize ) {
        throw new RuntimeException ( "Invalid file size [" + fileName + "]" );
    }

    sleepSec ( 1 );

    if ( deleteAfter ) {
        unlink ( fileName );
    }

    sleepSec ( 1 );
}   /* createFile () */

protected
void
createDir ( String dirName )
{
    System.out.printf ( "createDir [%s]\n", dirName );
    runWrapCmd ( "CD " + dirName, false );
}   /* createDir () */

protected
void
copyFile ( String from, String to )
{
    System.out.printf ( "copyFile [%s] to [%s]\n", from, to );
    runWrapCmd ( "CP " + from + " " + to, false );
}   /* createFile () */

protected
boolean
compareFiles ( String file1, String file2 )
{
    try {
        System.out.printf ( "compareFiles [%s] and [%s]\n", file1, file2 );
        runWrapCmd ( "CM " + file1 + " " + file2, false );
        return true;
    }
    catch ( Throwable Th ) { }

    return false;
}   /* compareFiles () */

class FileStat {
    FileStat () 
    {
        _type = null;
        _size = 0;
    }

    FileStat ( String line ) {
        StringTokenizer st = new StringTokenizer ( line );
        _type = st.nextToken ();
        _size = Integer.parseInt ( st.nextToken () );

        if ( ! _type.equals ( "F" ) && ! _type.equals ( "D" ) ) {
            throw new RuntimeException ( "Invalid file tupe [" + _type + "]" );
        }
    }

    protected String _type;
    protected int _size;
};

protected
FileStat
fileStat ( String path )
{
    String [] ar = runWrapCmd ( "FS " + path, true );
    if ( ar.length == 1 ) {
        return new FileStat ( ar [ 0 ] );
    }

    throw new RuntimeException ( "Error: invalid [FS] command output format" );
}   /* fileStat () */

protected
int
fileSize ( String path )
{
    System.out.printf ( "fileSize [%s]\n", path );
    return fileStat ( path )._size;
}   /* fileSize () */

protected
boolean
isDirectory ( String path )
{
    System.out.printf ( "isDirectory [%s]\n", path );
    return fileStat ( path )._type.equals ( "D" );
}   /* isDirectory () */

protected
void
list ( String path )
{
    System.out.printf ( "list [%s]\n", path );
    String [] the_list = runWrapCmd ( "LS " + path, true );
    if ( the_list != null ) {
        for ( int llp = 0; llp < the_list.length; llp ++ ) {
            System.out.printf ( "%s\n", the_list [ llp ] );
        }
    }
}   /* list () */

protected
void
unlink ( String path )
{
    System.out.printf ( "unlink [%s]\n", path );
    runWrapCmd ( "UL " + path, false );
}   /* unlink () */

};
