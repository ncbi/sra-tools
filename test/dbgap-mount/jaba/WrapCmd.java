package jaba;

import java.nio.file.*;
import java.io.*;
import java.util.Arrays;

public
class
WrapCmd {

final static String _sCreateDir = "CD";
final static String _sCreateFile = "CF";
final static String _sCopyFile = "CP";
final static String _sCompFiles = "CM";
final static String _sFileStat = "FS";
final static String _sList = "LS";
final static String _sUnlink = "UL";

private static
boolean
checkIfHelp ( String args [] )
{
    if ( args.length <= 1 ) {
        return true;
    }

    for ( int llp = 0; llp < args.length; llp ++ ) {
        if ( args [ llp ].equals ( "-h" ) || args [ llp ].equals ( "--help" ) ) {
            return true;
        }
    }

    return false;
}   /* checkIfHelp () */

public static
void
main( String args [] ) throws Throwable
{
    if ( checkIfHelp ( args ) ) {
        printHelp ();
    }

    String Cmd = args [ 0 ];

    while ( true ) {
        if ( Cmd.equals ( _sCreateDir ) ) {
            if ( args.length != 2 ) { printHelp (); }

            createDir ( args [ 1 ] );

            break;
        }

        if ( Cmd.equals ( _sCreateFile ) ) {
            if ( args.length != 3 ) { printHelp (); }

            createFile ( args [ 1 ], Integer.parseInt ( args [ 2 ] ) );

            break;
        }
        
        if ( Cmd.equals ( _sCopyFile ) ) {
            if ( args.length != 3 ) { printHelp (); }

            copyFile ( args [ 1 ], args [ 2 ] );

            break;
        }
        
        if ( Cmd.equals ( _sCompFiles ) ) {
            if ( args.length != 3 ) { printHelp (); }

            compareFiles ( args [ 1 ], args [ 2 ] );

            break;
        }
        
        if ( Cmd.equals ( _sFileStat ) ) {
            if ( args.length != 2 ) { printHelp (); }

            fileStat ( args [ 1 ] );

            break;
        }
        
        if ( Cmd.equals ( _sList ) ) {
            if ( args.length != 2 ) { printHelp (); }

            list ( args [ 1 ] );

            break;
        }
        
        if ( Cmd.equals ( _sUnlink ) ) {
            if ( args.length != 2 ) { printHelp (); }

            unlink ( args [ 1 ] );

            break;
        }

        throw new RuntimeException ( "Invalid command [" + Cmd + "]" );
    }


}

public static
void
printHelp ()
{
        /* Not sure what is better :LOL: */
    String className = new Object() { }.getClass().getEnclosingClass().getName ();
    // String className = new Object() { }.getClass().getEnclosingClass().getSimpleName ();

    System.out.printf (
                        "\nSyntax: java %s command command_argument(s)\n",
                        className
                        );
    System.out.printf ( "\nWhere:\n\n" );
    System.out.printf ( "   command - one of %s, %s, %s, %s, %s, %s or %s\n", _sCreateDir, _sCreateFile, _sCopyFile, _sCompFiles, _sFileStat, _sList, _sUnlink );

    System.out.printf ( "\nAvailabe commands and arguments:\n\n" );
    System.out.printf ( "Create Directory:" );
    System.out.printf ( "   %s directory_name\n", _sCreateDir );
    System.out.printf ( "     Create File:" );
    System.out.printf ( "   %s file_name file_size\n", _sCreateFile );
    System.out.printf ( "       Copy File:" );
    System.out.printf ( "   %s file_from file_to\n", _sCopyFile );
    System.out.printf ( "   Compare Files:" );
    System.out.printf ( "   %s file1 file2\n", _sCompFiles );
    System.out.printf ( "       File Stat:" );
    System.out.printf ( "   %s file_name\n", _sFileStat );
    System.out.printf ( "List Recursively:" );
    System.out.printf ( "   %s directory_name\n", _sList );
    System.out.printf ( "Unlink File or Directory:" );
    System.out.printf ( "   %s name\n", _sUnlink );
    System.out.printf ( "\n" );

    throw new RuntimeException ( "Invalid arguments" );
}   /* printHelp () */

private static
void
createDir ( String dirName ) throws IOException
{
    new File ( dirName ).mkdir ();
    // Files.createDirectory ( Paths.get ( dirName ) );
}   /* createDir () */

private static
void
createFile ( String fileName, int fileSize ) throws IOException
{
    String Line = "0123456789abcdef";
    byte [] Bytes = Line.getBytes ();

    // new File ( fileName ) . createNewFile ();

    FileOutputStream out = new FileOutputStream ( fileName );

    int acc_size = 0;

    while ( acc_size < fileSize ) {

        int size_2_write = fileSize - acc_size;
        if ( Bytes.length < size_2_write ) {
            size_2_write = Bytes.length;
        }

        out.write( Bytes, 0, size_2_write );

        acc_size += size_2_write;
    }

    out.close();
}   /* createFile () */

private static
void
copyFile ( String from, String to ) throws IOException
{
    Files.copy (
                new FileInputStream ( new File ( from ) ),
                Paths.get ( to )
                );
}   /* copyFile () */

private static
void
compareFiles ( String file1, String file2 ) throws IOException
{
    long fs1 = Files.size ( Paths.get ( file1 ) );
    long fs2 = Files.size ( Paths.get ( file2 ) );

    if ( fs1 == fs2 ) {
        byte [] bytes1 = Files.readAllBytes ( Paths.get ( file1 ) );
        byte [] bytes2 = Files.readAllBytes ( Paths.get ( file2 ) );

        if ( Arrays.equals ( bytes1, bytes2 ) == true ) {
            return;
        }
    }

    throw new RuntimeException ( "Files [" + file1 + "] and [" + file2 + "] are different" );
}   /* compareFiles () */

private static
void
fileStat ( String name ) throws IOException
{
    File file = new File ( name );

    if ( file.exists () ) {
        System.out.printf (
                        "%s\t%d\n",
                        ( file.isDirectory () ? "D" : "F" ),
                        Files.size ( file.toPath () )
                        );
        return;
    }

    throw new RuntimeException ( "File [" + name + "] does not exist" );
}   /* fileStat () */

private static
void
doList ( Path path ) throws IOException
{
    System.out.println ( path.toString () );

    if ( Files.isDirectory ( path ) ) {
        DirectoryStream < Path > stream = Files.newDirectoryStream ( path );
        for ( Path entry : stream ) {
            doList ( entry );
        }
    }
}   /* doList () */

private static
void
list ( String name ) throws IOException
{
    Path path = Paths.get ( name );
    if ( Files.exists ( path ) ) {
        doList ( path );
        return;
    }
    throw new RuntimeException ( "File [" + name + "] does not exist" );
}   /* list () */

private static
void
doUnlink ( Path path ) throws IOException
{
    if ( Files.isDirectory ( path ) ) {
        DirectoryStream < Path > stream = Files.newDirectoryStream ( path );
        for ( Path entry : stream ) {
            doUnlink ( entry );
        }
    }

    // ThePath.toFile ().delete ();
    Files.delete ( path );
}   /* doUnlink () */

private static
void
unlink ( String name ) throws IOException
{
    Path path = Paths.get ( name );
    if ( Files.exists ( path ) ) {
        doUnlink ( path );
        return;
    }
    throw new RuntimeException ( "File [" + name + "] does not exist" );
}   /* unlink () */

};
