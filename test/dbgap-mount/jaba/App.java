package jaba;

import java.lang.Thread;
import java.lang.Runnable;
import java.lang.Process;
import jaba.platform.Platform;
import jaba.DbGapMountTestEnv;
import java.io.InputStreamReader;

public
class
App {

private String _command = null;
private String [] _env = null;
private AppTerminator _terminator = null;

private Thread _thread = null;
private Process _process = null;

private int _exitStatus = 0;
private boolean _finished = false;
private boolean _terminated = false;

public
App ( String command, String [] env )
{
    this ( command, env, null );
}   /* App () */

public
App ( String command, String [] env, AppTerminator terminator )
{
    if ( command == null ) {
        throw new RuntimeException ( "Application command is not defined" );
    }

    _command = command;
    _env = env;
    _terminator = terminator;

    _thread = null;
    _process = null;

    _exitStatus = 0;
    _finished = false;
    _terminated = false;
}   /* App () */

private
void
runApp ()
{
    _exitStatus = 0;
    _finished = false;
    _terminated = false;

    try {
        _process = Runtime.getRuntime ().exec ( _command, _env );
        if ( _process == null ) {
            throw new RuntimeException ( "Can not run command [" + _command + "]" );
        }

        _process.waitFor ();

        _exitStatus = _process.exitValue ();
    }
    catch ( Throwable Th ) {
        System.err.println ( "App failed because: " + Th.toString () );

        if ( _process != null ) {
            stopApp ();
        }
        _exitStatus = 666;
    } 

    _finished = true;
}   /* runApp () */

protected
void
stopApp ()
{ 
    if ( isRunning () ) {
        System.out.println ( "Terminating application [" + _command + "]" );
        boolean FailedTerminate = _terminator != null
                                    ? _terminator.gentleTerminate ()
                                    : false
                                    ;
        if ( FailedTerminate ) {
            _process.destroy ();
        }
    }
}   /* stopApp () */

public
InputStreamReader
getOutStream ()
{
    synchronized ( this ) {
        if ( _process == null ) {
            throw new RuntimeException ( "Can not get OutStream for NULL process" );
        }

        return new InputStreamReader( _process.getInputStream () );
    }
}   /* getOutStream () */

public
InputStreamReader
getErrStream ()
{
    synchronized ( this ) {
        if ( _process == null ) {
            throw new RuntimeException ( "Can not get ErrStream for NULL process" );
        }

        return new InputStreamReader( _process.getErrorStream () );
    }
}   /* getErrStream () */

public
boolean
isRunning ()
{
    synchronized ( this ) {
        return _process != null && _finished == false;
    }
}   /* isRunning () */

public
void
start ()
{
    start ( 0 );
}   /* start () */

public
void
start ( long timeoutSec )
{
    synchronized ( this ) {
        if ( _thread != null ) {
            throw new RuntimeException ( "Can not start allready running App" );
        }
        _thread = new Thread ( new Runnable () { public void run () { runApp (); } } );

        _thread.start ();
    }

    if ( timeoutSec != 0 ) {
        try {
            Thread thread = new Thread (
                                new Runnable () {
                                    public
                                    void
                                    run ()
                                    {
                                        Utils.sleepSec ( timeoutSec );
                                        stopApp ();
                                    };
                                }
                            );
            thread.start ();
            thread = null;
        }
        catch ( Throwable Th ) {
            System.out.println ( Th.toString () );
        }
    }
}   /* start () */

public
void
stop ()
{
    synchronized ( this ) {
        if ( _thread == null ) { 
            throw new RuntimeException ( "Can not stop allready stopped App" );
        }

        stopApp ();

        _thread = null;
    }
}   /* stop () */

public
void
waitFor ()
{
    try {
            /* No check for NULL, cuz ... we eat everything */
        _thread.join ();
    }
    catch ( Throwable Th ) {
        throw new RuntimeException ( "Can not wait: " + Th.toString () );
    }
}   /* waitFor () */

public
int
exitStatus ()
{
    if ( isRunning () ) {
        throw new RuntimeException ( "Can not get exit status from running application" );
    }

    return _terminated ? 111 : _exitStatus;
}   /* exitStatus () */

};
