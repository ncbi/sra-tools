package jaba.platform;

import java.lang.Exception;

public abstract
class
PlatformSuper
    implements Platform {

final static String _sDbGapMountTest = "DbGapMountTest";

private String _appDir = null;
protected String _fileSeparator = null;
protected String _home = null;

public
PlatformSuper ( String AppDir )
{
    System.out.println ( "Detected [" + getOS () + "] platform" );

    _appDir = AppDir;

    if ( _appDir == null ) {
        throw new RuntimeException ( "NULL AppDir passed for os [" + getOS () + "]" );
    }

    _fileSeparator = System.getProperty ( "file.separator" );
    if ( _fileSeparator == null ) {
        throw new RuntimeException ( "'file.separator' undefined for [" + getOS () + "]" );
    }

    _home = System.getProperty ( "user.home" );
    if ( _home == null ) {
        throw new RuntimeException ( "'user.home' undefined for [" + getOS () + "]" );
    }

}   /* PlatformSuper () */


    /*> General, returns a name of OS
     <*/
// public
// String
// getOS ();

/*>>>   Configuration and environment related thins
 <<<    We need to maintain individual configuration for test run.
  >>>   We will use directory $HOME/DbGapMountTest$PLATFORM as root
 <<<    That directory will contain following subdirectories :
  >>>       .ncbi - configuration directory  
 <<<        ncbi  - data directory
  >>>       mount - mount point
 <<<    Method getTestDir should return path to test root directory
  >>>   
 <<<    
  >>>   Somewhere on PanFs suppose to be NGC and KART files, so thereis
 <<<    getInputDataDir method to locate that place
  >>>   
 <<<    Somewhere else there should be binaries to test, and there is
  >>>   getAppDir method to locate that place
 <<<*/
    /*> Returns mountpoint directory
     <*/
public
String
getTestDir ()
{
    return   _home + _fileSeparator + _sDbGapMountTest + getOS ();
}   /* getTestDir () */

public
String
getInputDataDir ()
{
    return "/panfs/pan1/trace_work/iskhakov/IMPORTANT_DIR";
}   /* getInputDataDir () */

public
String
getAppDir ()
{
    return _appDir;
}   /* getAppDir () */

public
String
getTarget ( TargetName Name )
{
    switch ( Name ) {
        case Separator:
                return _fileSeparator;
        case ConfigDir:
                return  getTestDir () + _fileSeparator + ".ncbi";
        case DataDir:
                return  getTestDir () + _fileSeparator + "ncbi";
        case TempDir:
                return  getTestDir () + _fileSeparator + "temp";
        case ConfigFile:
                return    getTarget ( TargetName.ConfigDir )
                        + _fileSeparator
                        + "user-settings.mkfg"
                        ;
        case MountPoint:
                return  getTestDir () + _fileSeparator + "mount";
        case Project:
                return "6570";
        case NgcFile:
                return  getInputDataDir ()
                        + _fileSeparator
                        + _sDbGapMountTest
                        + _fileSeparator
                        + "prj_"
                        + getTarget ( TargetName.Project )
                        + ".ngc"
                        ;
        case KartFile:
                return  getInputDataDir ()
                        + _fileSeparator
                        + _sDbGapMountTest
                        + _fileSeparator
                        + "kart_prj_"
                        + getTarget ( TargetName.Project )
                        + ".krt"
                        ;
        case MountApp:
                return    getAppDir ()
                        + _fileSeparator
                        + "dbgap-mount-tool"
                        ;
        case ConfigApp:
                return    getAppDir ()
                        + _fileSeparator
                        + "vdb-config"
                        ;
        case DumpApp:
                return    getAppDir ()
                        + _fileSeparator
                        + "fastq-dump-new"
                        ;
    };

    throw new RuntimeException ( "Invalid tartet for [" + getOS () + "]" );
}   /* getTarget () */

    /* For some platforms we do not need directory created 
     */
public
boolean
needCreateMountPoint ()
{
    return true;
}   /* needCreateMountPoint () */

public
String []
getProcessEnvironment ()
{
    String [] RetVal = {
                    "HOME=" + _home,
                    "NCBI_HOME=" + getTarget ( TargetName.ConfigDir )
                    };

    return RetVal;
}   /* getProcessEnvironment () */

};
