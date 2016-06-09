package jaba.test;

import jaba.platform.Platform;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.util.Vector;
import java.lang.reflect.Constructor;

/* That class will open file Test.list in the same package,
 * read list of tests available
 */

public
class
Tests {


private Platform _platform = null;
private String [] _tests = {};

public
Tests ( Platform platform )
{
    if ( platform == null ) {
        throw new RuntimeException ( "NULL platform passed" );
    }

    _platform = platform;

    loadTests ();

}   /* Tests () */

public
void
run ( String TestName ) 
{
    if ( TestName == null ) {
        throw new RuntimeException ( "ERROR: NULL test name passed" );
    }

    Test test = testForName ( TestName );
    if ( test == null ) {
        throw new RuntimeException ( "ERROR: Can not find test for [" + TestName + "]" );
    }

    test.run ();

    if ( test.exitStatus () != 0 ) {
        throw new RuntimeException ( "ERROR: Test failed [" + TestName + "[" );
    }
}   /* run () */

public
void
run ()
{
    for ( int llp = 0; llp < _tests.length; llp ++ ) {
        run ( _tests [ llp ] );
    }
}   /* run () */

public
void
printTests ( boolean WithTickets )
{
}   /* printTests () */

public
void
printTickets ( boolean WithTests )
{
}   /* printTests () */

private
void
loadTests ()
{
    try { 
        String rName = getClass ().getSimpleName () + ".list";

        InputStream in = getClass ().getResourceAsStream ( rName );

        BufferedReader rd = new BufferedReader (
                        new InputStreamReader (
                            getClass ().getResourceAsStream ( rName )
                        )
                    );

        Vector < String > tests = new Vector < String > ();
        String line;
        while ( ( line = rd.readLine () ) != null ) {
            String test = parseLine ( line );

            if ( test != null ) {
                tests.addElement ( test );
            }
        }

        rd.close ();

        _tests = tests.toArray ( _tests );
    } 
    catch ( Throwable Th ) {
        throw new RuntimeException ( Th.toString () );
    }
}   /* loadTests () */

private
String
parseLine ( String line )
{
        /* Simple : trim and check if it does not start with "#"
         */
    String retVal = line.trim ();
    if ( retVal.isEmpty () ) {
        return null;
    }

    return retVal.charAt (0) == '#' ? null : retVal; 
}   /* parseLine () */

    /*>> We suppose that all Test classes have only one constructor
     <<  which required instance of 'jaba.platform.Platform' class
      >> We will load class by it's name then. Simple algo: if class
     <<  name contains '.' we suppose it is full name with package,
      >> overwise it will use name "java.test.Test##Name"
     <<*/
private
Test
testForName ( String Name )
{
    if ( Name == null ) {
        throw new RuntimeException ( "Test name missed for platform [" + _platform.getOS () + "]" );
    }

    try {
        String className = Name;
        if ( Name.indexOf ( '.' ) == 1 * - 1 ) {
            className = getClass ().getPackage().getName() + ".Test" + Name;
        }

        Class testClass = Class.forName ( className );
        if ( testClass == null ) {
            throw new RuntimeException ( "Can not find class for [" + className + "] on [" + _platform + "] platform" );
        }

        Constructor constructor = testClass.getConstructor (
                                                        Platform.class
                                                        );
        if ( constructor == null ) {
            throw new RuntimeException ( "Can not find constructor for class [" + className + "] on [" + _platform + "] platform" );
        }

        Object object = constructor.newInstance (
                                            new Object [] { _platform }
                                            );
        if ( object == null ) {
            throw new RuntimeException ( "Can not construct instance for class [" + className + "] on [" + _platform + "] platform" );
        }

        return ( Test ) object;
    }
    catch ( Throwable Th ) {
        throw new RuntimeException ( Th.toString () );
    }
}   /* testForName () */

};
