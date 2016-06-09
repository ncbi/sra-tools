import sys

# We should load appropriate module for appropriate OS
#

if sys.platform == 'linux2':
    from platform.lin import *
elif sys.platform == 'darwin':
    from platform.mac import *
elif sys.platform == 'win32':
    from platform.win import *
else:
    raise ImportError ( "Platfrom '%s' is not supported" %(sys.platform) )

