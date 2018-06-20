#! /usr/bin/env python

import sys
import os
import stat
import datetime
from itertools import izip

def perform( fn1, fn2 ) :
    print( "comparing: " )
    print( "\tA = %s" %fn1 )
    print( "\tB = %s" %fn2 )
    ln = 0
    A = [ "", "", "", "" ]
    B = [ "", "", "", "" ]
    with open( fn1 ) as f1, open( fn2 ) as f2 :
        for L1, L2 in izip( f1, f2 ) :
            A[ ln ] = L1.strip()
            B[ ln ] = L2.strip()
            ln += 1
            if ln > 3 :
                ln = 0
                if A != B :
                    if A[ 0 ] != B[ 0 ] :
                        print( "A[0]: %s" % A[ 0 ] )
                        print( "B[0]: %s" % B[ 0 ] )
                    else :
                        print( "*[0]: %s" % A[ 0 ] )

                    if A[ 1 ] != B[ 1 ] :
                        print( "A[1]: %s" % A[ 1 ] )
                        print( "B[1]: %s" % B[ 1 ] )
                    else :
                        print( "*[1]: %s" % A[ 1 ] )

                    if A[ 2 ] != B[ 2 ] :
                        print( "A[2]: %s" % A[ 2 ] )
                        print( "B[2]: %s" % B[ 2 ] )
                    else :
                        print( "*[2]: %s" % A[ 2 ] )

                    if A[ 3 ] != B[ 3 ] :
                        print( "A[3]: %s" % A[ 3 ] )
                        print( "B[3]: %s" % B[ 3 ] )
                    else :
                        print( "*[3]: %s" % A[ 3 ] )
                    print( "" )

if __name__ == '__main__':
    if len( sys.argv ) > 2 :
        fn1 = sys.argv[ 1 ]
        fn2 = sys.argv[ 2 ]
        if os.path.isfile( fn1 ) :
            if os.path.isfile( fn2 ) :
                perform( fn1, fn2 )
            else :
                print( "'%s' does not exist!" % fn2 )
        else :
            print( "'%s' does not exist!" % fn1 )
    else :
        print( "usage: fastq-diff.py file1.fastq file2.fastq" )
