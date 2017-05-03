package jaba.platform;

public
class 
PlatformWin
    extends PlatformSuper {

public
PlatformWin ( String AppDir )
{
    super ( AppDir );
}   /* PlatformWin () */

public
String
getOS ()
{
    return "Windows";
}   /* getOS () */

public
String
getInputDataDir ()
{
    return "\\\\panfs\\pan1\\trace_work\\iskhakov\\IMPORTANT_DIR";
}   /* getInputDataDir () */

    /*> Redefining, because under windows we tend to new VOLUME
     <*/
public
String
getTarget ( TargetName Name )
{
    switch ( Name ) {
        case MountPoint:
                return "r:";
        case MountApp:
                return    getAppDir ()
                        + _fileSeparator
                        + "dbgap-mount-tool.exe"
                        ;
        case ConfigApp:
                return    getAppDir ()
                        + _fileSeparator
                        + "vdb-config.exe"
                        ;
        case DumpApp:
                return    getAppDir ()
                        + _fileSeparator
                        + "fastq-dump-ngs.exe"
                        ;
        case UmountApp:
                return "c:\\\\ProgramFiles (x86)\\Dokan\\DokanLibrary\\dokanctl.exe /u";
    };

    return super.getTarget ( Name );
}   /* getTarget () */

public
boolean
needCreateMountPoint ()
{
    return false;
}   /* needCreateMountPoint () */

public
String []
getProcessEnvironment ()
{
    String [] RetVal = {
                    "USERPROFILE=" + _home,
                    "NCBI_HOME=" + getTarget ( TargetName.ConfigDir )
                    };

    return RetVal;
}   /* getProcessEnvironment () */

};
