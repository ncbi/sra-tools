import sys
import os.path
import subprocess

if len ( sys.argv ) != 2 :
    print ( "\n" )
    print ( "This program will check correct program behaviour when it called" )
    print ( "without arguments, or with argument '-h'. In the first case it" )
    print ( "should print help message and exit non zero code. In the second" )
    print ( "case it should print help message and exit with zero code.\n" )

    print ( "Invalid arguments\n" )
    print ( "Usage:\n" )
    print ( "  " + os.path.basename ( __file__ ) + " program_path\n" )
    print ( "Where:\n" );
    print ( "  program_path - path to program to test\n" );

    exit ( 1 )

##
## First checking if file exists and executable
##

BANANA = sys.argv [ 1 ]

if not os.path.exists ( BANANA ):
    print ( "\nERROR: Can not stat file [" + BANANA + "]\n" )
    exit ( 1 )

if not os.access ( BANANA, os.X_OK ):
    print ( "\nERROR: File [" + BANANA + "] is not an executable\n" )
    exit ( 1 )


def check_stream ( stream ):
    for line in stream:
        if line.startswith ( "Usage:" ):
            return True
    return False

##
## Run command without arguments: should return non zero RC and print
## help message.
##
print ( "======================================" )
print ( "TEST1: start command without arguments" )
print ( "--------------------------------------" )

popca = subprocess.Popen (
                    [ BANANA ],
                    stdout = subprocess.PIPE,
                    stderr = subprocess.PIPE
                    )

in_out = check_stream ( popca.stdout )
in_err = check_stream ( popca.stderr )
rc = popca.wait ()

if not in_out and not in_err:
    print ( "ERROR[TEST1] no help message" )
    exit ( 1 )

if rc == 0:
    print ( "ERROR[TEST1] return code is '0'" )
    exit ( 1 )

if in_out:
    print ( "WARNING[TEST1] help message is in 'STDOUT'" )

print ( "Help message detected" )
print ( "Return code is [" + str ( rc ) + "]" );

print ( "Passed\n" )

##
## Run command with '-h' argument: should return zero RC and print
## help message.
##
print ( "=======================================" )
print ( "TEST2: start command with '-h' argument" )
print ( "---------------------------------------" )

popca = subprocess.Popen (
                    [ BANANA, "-h" ],
                    stdout = subprocess.PIPE,
                    stderr = subprocess.PIPE
                    )

in_out = check_stream ( popca.stdout )
in_err = check_stream ( popca.stderr )
rc = popca.wait ()

if not in_out and not in_err:
    print ( "ERROR[TEST2] no help message" )
    exit ( 1 )

if not rc == 0:
    print ( "ERROR[TEST2] return code is '" + str ( rc ) + "'" )
    exit ( 1 )

if in_err:
    print ( "WARNING[TEST2] help message is in 'STDERR'" )

print ( "Help message detected" )
print ( "Return code is [" + str ( rc ) + "]" );

print ( "Passed\n" )
