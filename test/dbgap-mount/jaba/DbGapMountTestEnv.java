package jaba;

import jaba.platform.Platform;

import java.nio.file.*;
import java.io.OutputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.InputStreamReader;

public
class 
DbGapMountTestEnv {

private Platform _platform = null;

public enum EnvType { Undef, NoConf, BadConf, GoodConf };
private EnvType envType = EnvType.Undef;

private String pathSeparator = null;

private String vdbConfig = null;
private String mountTool = null;
private String fastqDump = null;


public
DbGapMountTestEnv ( Platform Platform )
{
    if ( Platform == null ) {
        throw new RuntimeException ( "Undefined platform" );
    }

    _platform  = Platform;
}   /* DbGapMountTestEnv () */

public
void
createTestEnv ( EnvType EnvType )
{
    if ( envType != EnvType.Undef ) {
        throw new RuntimeException ( "Can not create environment. Clear old first" );
    }

    if ( _platform == null ) {
        throw new RuntimeException ( "Can not create environment. NULL Platform or BinDir passed" );
    }

    pathSeparator = System.getProperty ( "file.separator" );

        /*  Removing data from previous experiments
         */
    Utils.unlink ( _platform.getTestDir () );

        /*  Checking existance necessary files
         */
    checkExecutables ();

        /*  Checking existance necessary other files
         */
    checkInputs ();

        /*  Creating file tree
         */
    makeFileTree ();

        /*  Configuring ();
         */
    configure ( EnvType );
}   /* createTestEnv () */

public
void
clearTestEnv ()
{
    if ( envType == EnvType.Undef ) {
        return;
    }

    if ( _platform == null ) {
        throw new RuntimeException ( "Can not clear environment. NULL Platform or BinDir passed" );
    }

        /*  Removing unnecessary data
         */
    Utils.unlink ( _platform.getTestDir () );

    envType = EnvType.Undef;

}   /* clearTestEnv () */

private
void
makeFileTree ()
{
    System.out.format ( "Making file tree\n" );

    Utils.mkdir (
                _platform.getTestDir ()
                );
    Utils.mkdir (
                _platform.getTarget ( Platform.TargetName.ConfigDir )
                );
    Utils.mkdir (
                _platform.getTarget ( Platform.TargetName.DataDir )
                );
    Utils.mkdir (
                _platform.getTarget ( Platform.TargetName.TempDir )
                );
    if ( _platform.needCreateMountPoint () ) {
        Utils.mkdir (
                _platform.getTarget ( Platform.TargetName.MountPoint )
                );
    }
    else {
        System.out.format ( "Does not need mount point to exist [%s]\n", _platform.getTarget ( Platform.TargetName.MountPoint ) ); 
    }
}   /* makeFileTree () */

private 
void
checkExecutables ()
{
    System.out.format ( "Checking executables\n" );

    Utils.checkExecutable (
                _platform.getTarget ( Platform.TargetName.MountApp )
                );
    Utils.checkExecutable (
                _platform.getTarget ( Platform.TargetName.ConfigApp )
                );
    Utils.checkExecutable (
                _platform.getTarget ( Platform.TargetName.DumpApp )
                );
}   /* checkExecutables () */

private
void
checkInputs ()
{
    System.out.format ( "Checking inputs\n" );

    Utils.checkExists (
                _platform.getTarget ( Platform.TargetName.NgcFile )
                );
    Utils.checkExists (
                _platform.getTarget ( Platform.TargetName.KartFile )
                );
}   /* checkInputs () */

private
void
configure ( EnvType Type )
{
    System.out.format ( "Configuring\n" );

    if ( _platform == null ) {
        throw new RuntimeException ( "Can not configure environment. NULL Platform or BinDir passed" );
    }

    if ( Type == EnvType.Undef ) {
        throw new RuntimeException ( "Invalid environment type passed" );
    }

    envType = Type;

    if ( envType == EnvType.NoConf ) {
        /*  just return
         */
        return;
    }

        /*  Here we do create config file
         */
    try {
        String ThePath = _platform.getTarget (
                                    Platform.TargetName.ConfigFile
                                    );
        OutputStream Out = new BufferedOutputStream (
                        Files.newOutputStream (
                                Paths.get ( ThePath ),
                                StandardOpenOption.CREATE,
                                StandardOpenOption.TRUNCATE_EXISTING
                                )
                        );
        String Jojoba = "## auto-generated configuration file - DO NOT EDIT ##\n\n";
        byte Bytes [] = Jojoba.getBytes ();

        Out.write ( Bytes, 0, Bytes.length );

        Jojoba = "/repository/user/default-path = \""
                    + Utils.pathToPosix ( _platform.getTarget ( Platform.TargetName.DataDir ) )
                    + "\"\n"
                    ;
        Bytes = Jojoba.getBytes ();

        Out.write ( Bytes, 0, Bytes.length );

        Out.close ();

    }
    catch ( Throwable Th ) {
        throw new RuntimeException ( "ERROR: Can not write config: " + Th.toString () );
    }

    if ( envType == EnvType.BadConf ) {
            /* We just return after dummy config creation
             */
        return;
    }

    if ( envType == EnvType.GoodConf ) {
        /*  Here we import NGC file
         */
        try {
            String Cmd =   _platform.getTarget (
                                        Platform.TargetName.ConfigApp
                                        )
                         +  " --import "
                         + _platform.getTarget (
                                        Platform.TargetName.NgcFile
                                        );

            Process proc = Runtime.getRuntime().exec ( Cmd, _platform.getProcessEnvironment() );
            proc.waitFor ();

            Utils.printProcessOut ( proc );
            Utils.printProcessErr ( proc );

        }
        catch ( Throwable Th ) {
            throw new RuntimeException ( "ERROR: Can not write config: " + Th.toString () );
        }

        return;
    }

    throw new RuntimeException ( "ERROR: Unknown environment type" ); 
}   /* configure () */

};

