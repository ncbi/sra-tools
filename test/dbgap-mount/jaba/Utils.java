package jaba;

import java.nio.file.*;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.util.Arrays;

/*///
 \\\    This file contains some simplicied methods
 ///*/
public
class
Utils {

private static
void
doUnlink ( Path ThePath )
    throws IOException
{
    if ( Files.isDirectory ( ThePath ) ) {
        try ( DirectoryStream < Path > stream = Files.newDirectoryStream ( ThePath ) ) {
            for ( Path entry : stream ) {
                doUnlink ( entry );
            }
        }
        catch ( Throwable Th ) {
            throw new RuntimeException ( Th.toString () );
        }
    }

    Files.delete ( ThePath );
}   /* doUnlink () */

public static
void
unlink ( String ThePath )
{
    if ( ThePath == null ) {
        throw new RuntimeException ( "Error: NULL path passed" );
    }

    System.out.format ( "Deleting: %s\n", ThePath );

    try {
        Path FilePath = Paths.get ( ThePath );
        if ( Files.exists ( FilePath ) ) {
            doUnlink ( FilePath );
        }
    }
    catch ( Throwable Th ) {
        throw new RuntimeException ( " Error: can not unlink [" + ThePath + "] because [" + Th.toString () + "]" );
    }
}   /* unlink () */

public static
void
mkdir ( String ThePath )
{
    if ( ThePath == null ) {
        throw new RuntimeException ( "Error: NULL path passed" );
    }

    System.out.format ( "Creating Dir: %s\n", ThePath );

    try {
        Files.createDirectory ( Paths.get ( ThePath ) );
    }
    catch ( Throwable Th ) {
        throw new RuntimeException ( " Error: can not create directory [" + ThePath + "] because [" + Th.toString () + "]" );
    }
}   /* mkdir () */

public static
void
checkExecutable ( String ThePath )
{
    if ( ThePath == null ) {
        throw new RuntimeException ( "Error: NULL path passed" );
    }

    if ( ! Files.isExecutable ( Paths.get ( ThePath ) ) ) {
        throw new RuntimeException ( " Error: can not stat executable [" + ThePath + "]" );
    }

    System.out.format ( "Found Binary [%s]\n", ThePath );
}   /* checkExecutable () */

public static
void
checkExists ( String ThePath )
{
    if ( ThePath == null ) {
        throw new RuntimeException ( "Error: NULL path passed" );
    }

    if ( ! Files.exists ( Paths.get ( ThePath ) ) ) {
        throw new RuntimeException ( "Error: can not stat executable [" + ThePath + "]" );
    }

    System.out.format ( "Found File [%s]\n", ThePath );
}   /* checkExists () */

/*\ Dull converter of Windows notation to POSYX. No mex - no chex
 \*/
public static
String
pathToPosix ( String Path )
{
        /* All what we do is replacing second ':' to nothing
         * and replacing all '\' with '/'.
         * I totally agree, it is not correct, but it is enough
         * for that test.
         */

        /*  First, we check if string starts from 'x:'
         */
    String RetVal;

    if ( Path.matches ( "^[a-z,A-Z][:]\\p{Graph}*" ) ) {
        RetVal = Path.substring ( 2 );
        if ( RetVal.charAt ( 0 ) == '\\' ) {
            RetVal = "/" + Path.charAt ( 0 ) + RetVal;
        }
        else {
            RetVal = "/" + Path.charAt ( 0 ) + "/" + RetVal;
        }
    }
    else {
        RetVal = Path;
    }

    RetVal = RetVal.replace ( '\\', '/' );

    return RetVal;
}   /* pathToPosix () */

public static
void
printProcessOut ( Process Proc )
{
    if ( Proc == null ) {
        return;
    }

    try {
        System.out.println ( "||<=== LOG ====" );
        BufferedReader out = new BufferedReader (
                        new InputStreamReader( Proc.getInputStream () )
                        );
        String line;
        while ( ( line = out.readLine() ) != null ) {
            System.out.println(line);
        }

        out.close ();
        System.out.println ( "||<============" );
    }
    catch ( Throwable Th ) {
        throw new RuntimeException ( Th.toString () );
    }
}   /* printProcessOut () */

public static
void
printProcessErr ( Process Proc )
{
    if ( Proc == null ) {
        return;
    }

    int exitValue = Proc.exitValue ();
    if ( exitValue == 0 ) {
        return;
    }

    try {
        System.out.format ( "||<=== ERR [%s] ====\n", exitValue );
        BufferedReader out = new BufferedReader (
                        new InputStreamReader( Proc.getErrorStream () )
                        );
        String line;
        while ( ( line = out.readLine() ) != null ) {
            System.out.println(line);
        }

        out.close ();
        System.out.println ( "||<============" );
    }
    catch ( Throwable Th ) {
        throw new RuntimeException ( Th.toString () );
    }
}   /* printProcessErr () */

public static
void
sleepSec ( long sec )
{
    try { Thread.sleep ( 1000 * sec ); } catch ( Throwable Th ) { };
}   /* sleepSec () */

public static
long
fileSize ( String name )
{
    long retVal = 0;
    try {
        System.out.printf ( "Retrieving size for file [%s]\n", name );

        retVal = Files.size ( Paths.get ( name ) );

        System.out.printf ( "File [%s] has size [%d]\n", name, retVal );
    }
    catch ( Throwable Th ) {
        System.out.printf ( "ERROR: can not retrieve size for file [%s]\n", name );
        throw new RuntimeException ( "Error: " + Th.toString () );
    }

    return retVal;
}   /* fileSize () */

public static
void
deleteFile ( String name )
{
    try {
        System.out.printf ( "Deleting file [%s]\n", name );

        Files.delete ( Paths.get ( name ) );

        System.out.printf ( "File deleted [%s]\n", name );
    }
    catch ( Throwable Th ) {
        System.out.printf ( "ERROR: can not delete file [%s]\n", name );
        throw new RuntimeException ( "Error: " + Th.toString () );
    }
}   /* deleteFile () */

public static
void
createDir ( String path )
{
    try {
        System.out.printf ( "Creating directory [%s]\n", path );

        Files.createDirectory ( Paths.get ( path ) );

        System.out.printf ( "Directory created [%s]\n", path );
    }
    catch ( Throwable Th ) {
        System.out.printf ( "ERROR: can not create directory [%s]\n", path );
        throw new RuntimeException ( "Error: " + Th.toString () );
    }
}   /* createDir () */

public static
void
createFile ( String name, int size )
{
    System.out.printf ( "Creating file [%s] with size [%d]\n", name, size );

    String Line = "0123456789abcdef";
    byte [] Bytes = Line.getBytes ();

    try {
        FileOutputStream out = new FileOutputStream ( name );

        int acc_size = 0;

        while ( acc_size < size ) {

            int size_2_write = size - acc_size;
            if ( Bytes.length < size_2_write ) {
                size_2_write = Bytes.length;
            }

            out.write( Bytes, 0, size_2_write );

            acc_size += size_2_write;
        }

        out.close();

        System.out.printf ( "File created [%s]\n", name );

        Utils.sleepSec ( 1 );
    }
    catch ( Throwable Th ) {
        System.out.printf ( "ERROR: can not create file [%s]\n", name );
        throw new RuntimeException ( "Error: " + Th.toString () );
    }
}   /* createFile () */

public static
void
copyFile ( String from, String to )
{
    try {
        System.out.printf ( "Copy file [%s] to [%s]\n", from, to );

            /* Had to use FileInputStream cuz under windows java.nio
             * does not work well with direct access to random disks
             */
        Files.copy (
                new FileInputStream ( new File ( from ) ),
                Paths.get ( to )
                );

        System.out.printf ( "File copied [%s] to [%s]\n", from, to );
    }
    catch ( Throwable Th ) {
        System.out.printf ( "ERROR: can not copy file [%s] to [%s]\n", from, to );
        throw new RuntimeException ( "Error: " + Th.toString () );
    }
}   /* copyFile () */

public static
boolean
compareFiles ( String file1, String file2 )
{
    System.out.printf ( "Comparing files [%s] and [%s]\n", file1, file2 );
    try {
        long fs1 = fileSize ( file1 );
        long fs2 = fileSize ( file2 );

        if ( fs1 == fs2 ) {
            byte [] bytes1 = Files.readAllBytes ( Paths.get ( file1 ) );
            byte [] bytes2 = Files.readAllBytes ( Paths.get ( file2 ) );

            if ( Arrays.equals ( bytes1, bytes2 ) == true ) {
                System.out.printf ( "EQUAL files [%s] and [%s]\n", file1, file2 );
                return true;
            }
        }
    }
    catch ( Throwable Th ) {
        System.out.println ( "Error: " + Th.toString () );

        throw new RuntimeException ( "Error: can not compare files [" + file1 + "] and [" + file2 + "]" );
    }

    System.out.printf ( "DIFFER files [%s] and [%s]\n", file1, file2 );

    return false;
}   /* compareFiles () */

private static
void
doList ( Path path )
    throws IOException
{
    String sig = "[F]";

    if ( Files.isDirectory ( path ) ) {
        try ( DirectoryStream < Path > stream = Files.newDirectoryStream ( path ) ) {
            for ( Path entry : stream ) {
                doList ( entry );
            }
        }
        catch ( Throwable Th ) {
            throw new RuntimeException ( Th.toString () );
        }

        sig = "[D]";
    }

    System.out.printf ( "%s%s\n", sig, path.toString () );

}   /* doList () */

public static
void
list ( String path )
{
    if ( path == null ) {
        throw new RuntimeException ( "Error: NULL path passed" );
    }

    System.out.format ( "Listing of [%s]\n", path );

    try {
        Path the_path = Paths.get ( path );
        if ( Files.exists ( the_path ) ) {
            doList ( the_path );
        }
    }
    catch ( Throwable Th ) {
        throw new RuntimeException ( " Error: can not list [" + path + "] because [" + Th.toString () + "]" );
    }
}   /* list () */

};
