import sys

import platform

import test

if len ( sys.argv ) != 2 :
    print ( "Invalid arguments\n" )
    exit ( 1 )

plt = platform.Platform ( sys.argv [ 1 ] )

# plt.about ()

tst = test.Test ( plt )

tst.run ()
