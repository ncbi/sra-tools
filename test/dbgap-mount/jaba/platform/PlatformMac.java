package jaba.platform;

public
class 
PlatformMac
    extends PlatformSuper {

public
PlatformMac ( String AppDir )
{
    super ( AppDir );
}   /* PlatformMac () */

public
String
getOS ()
{
    return "Mac";
}   /* getOS () */

public
String
getInputDataDir ()
{
    return "/net/pan1/trace_work/iskhakov/IMPORTANT_DIR";
}   /* getInputDataDir () */

public
String
getTarget ( TargetName Name )
{
    switch ( Name ) {
        case UmountApp:
            return "/sbin/umount";
    }

    return super.getTarget ( Name );
}   /* getTarget () */

};
