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
class TestWorkspace
    extends TestBasicBase {

public
TestWorkspace ( Platform platform )
{
    super ( platform );
}   /* TestWorkspace () */

public
String []
tickets ()
{
    String [] r = {
              "VDB-1528"
            , "VDB-1527"    /* Dokan module for bi-directional
                             * encryption on Windows
                             */
            , "VDB-1526"    /* FUSE module for bi-directional
                             * encryption on Linux and Mac
                             */
            , "VDB-1563"    /* Debug directory-level of FUSE/Dokan
                             * implementation
                             */
            , "VDB-2701"    /* Implement Read Only mode for
                             * encrypted file sytem and remove
                             * from Cache directory
                             */
            };
    return r;
}   /* tickets () */

    /* The ticket VDB-1563 is main issue: if size of written file
     * is fraction of 32K, file get corrupted. So, we check it.
     * Start Tool Daemon. Create file 32K, write. Stop Tool.
     * Start Tool Daemon and compare files. Stop tool.
     * That chould 
     */
protected
void
runActualTest ()
{

    String dirPath = pathToFile ( "dir_1" );
    String dirPathFile =
                  dirPath
               + _platform.getTarget ( Platform.TargetName.Separator )
               + ".nested_file"
               ;

        /* Creatin' and manipulating files
         */
    try {
        start ( false );

            /* Creatin' random file and deleting it
             */
        createFile ( pathToFile ( "file_0" ), 33, true );

            /* Creatin' 32K file and smaller file
             */
        createFile ( pathToFile ( "file_1" ), 32768, false );

            /* Makin' copy of file
             */
        copyFile (
                pathToFile ( "file_1" ),
                pathToTempFile ( "file_1" )
                );

            /* Checkin' copy of file
             */
        if ( ! compareFiles (
                pathToFile ( "file_1" ),
                pathToTempFile ( "file_1" )
                ) ) {
            throw new RuntimeException ( "ERROR: copied files are different [" + pathToFile ( "file_1" ) + "] and [" + pathToTempFile ( "file_1" ) + "]" );
        }

            /* Creatin' deletin' directory and nested file
             */
        createDir ( dirPath );
        createFile ( dirPathFile, 33, false );
        unlink ( dirPath );

            /* Creatin' directory and nested file
             */
        createDir ( dirPath );
        createFile ( dirPathFile, 33, false );
        list ( pathToFile ( "" ) );

        stop ();
    }
    catch ( Throwable Th ) {
        stop ();

        System.out.println ( "ERROR: " + Th.toString () );

        throw Th;
    }

    sleepSec ( 2 );

if ( true ) return;

        /* Checkin' RO mode and validity of workspace files
         */
    try {
        start ( true );

            /* Checkin' content of file one more time after remount
             */
        if ( ! compareFiles (
                pathToFile ( "file_1" ),
                pathToTempFile ( "file_1" )
                ) ) {
            throw new RuntimeException ( "ERROR: copied files are different [" + pathToFile ( "file_1" ) + "] and [" + pathToTempFile ( "file_1" ) + "]" );
        }

            /* Checkin' if we are able to create file
             */
        boolean wrong = false;
        try {
            createFile ( "file_0", 33, true );
            wrong = true;
        } catch ( Throwable Th ) { /* That's is OK */ }
        if ( wrong ) {
            throw new RuntimeException ( "ERROR: was able to create file [file_0]" );
        }

            /* Checkin' if we are able to delete file
             */
        wrong = false;
        try {
            unlink ( pathToFile ( "file_1" ) );
            wrong = true;
        } catch ( Throwable Th ) { /* That's is OK */ }
        if ( wrong ) {
            throw new RuntimeException ( "ERROR: was able to delete file [file_1]" );
        }

            /* Checking' directory and nested file
             */
        if ( ! Files.exists ( Paths.get ( dirPath ) ) ) {
            throw new RuntimeException ( "ERROR: can not find directory [dir_1]" );
        }
        if ( ! Files.exists ( Paths.get ( dirPathFile ) ) ) {
            throw new RuntimeException ( "ERROR: can not find file [dir_1/.nested_file]" );
        }
        list ( pathToFile ( "" ) );

        stop ();
    }
    catch ( Throwable Th ) {
        stop ();

        System.out.println ( "ERROR: " + Th.toString () );

        throw Th;
    }
}

};
