package jaba.test;

import jaba.platform.Platform;
import java.lang.reflect.Constructor;

public
interface
Test {

public
String
getName ();

    /*>>  That method could return NULL or empty length array,
     <<  it will be interpreted as ALL
      >>
     <<*/
public
String []
tickets ();

public
void
run ();

public
int
exitStatus ();

};
