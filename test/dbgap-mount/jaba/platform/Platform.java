package jaba.platform;

import java.lang.Exception;

/*|| That interface contains platform dependable code, like mount point
  || commands to launch process or what ever. Most of methods in that
  || interface are 'default' cuz I do not like to implement all methods
  || on all platforms in parallel
  || Will make it Serializable or Externiolizable later
  ||*/
public
interface
Platform {

public
enum
TargetName {
    Separator,
    ConfigDir,
    DataDir,
    TempDir,
    MountPoint,
    ConfigFile,
    Project,
    NgcFile,
    KartFile,
    MountApp,
    ConfigApp,
    DumpApp,
    UmountApp
};

    /*> General, returns a name of OS
     <*/
public
String
getOS ();

public
String
getTestDir ();

public
String
getInputDataDir ();

public
String
getAppDir ();

public
String
getTarget ( TargetName Name );

public
boolean
needCreateMountPoint ();

public
String []
getProcessEnvironment ();

    /*> Returns instance of Platform interface
     <*/
public static
Platform
getPlatform ( String AppDir )
{
    String OS = System.getProperty ( "os.name" );
    if ( OS.startsWith ( "Lin" ) ) {
        return new PlatformLinux ( AppDir );
    }

    if ( OS.startsWith ( "Mac" ) ) {
        return new PlatformMac ( AppDir );
    }

    if ( OS.startsWith ( "Win" ) ) {
        return new PlatformWin ( AppDir );
    }

    throw new RuntimeException ( "Unknown platform [" + OS + "]" );
}   /* getPlatform () */

};
