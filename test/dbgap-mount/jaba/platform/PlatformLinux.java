package jaba.platform;

public
class 
PlatformLinux
    extends PlatformSuper {

public
PlatformLinux ( String AppDir )
{
    super ( AppDir );
}   /* PlatformLinux () */

public
String
getOS ()
{
    return "Linux";
}   /* getOS () */

public
String
getTarget ( TargetName Name )
{
    switch ( Name ) {
        case UmountApp:
            return "/bin/fusermount -u";
    }

    return super.getTarget ( Name );
}   /* getTarget () */

};
