#! /usr/bin/env python3

import sys
import os

def perform( fn ) :
    ln = 0
    A = [ "", "" ]
    with open( fn ) as f :
        for line in f :
            A[ ln ] = line.strip()
            ln += 1
            if ln > 1 :
                print( "{} {}".format( A[0], A[1] ) )
                ln = 0

if __name__ == '__main__':
    if len( sys.argv ) > 1 :
        fn = sys.argv[ 1 ]
        if os.path.isfile( fn ) :
            perform( fn )
        else :
            print( "'%s' does not exist!" % fn )
    else :
        print( "usage: fasta_2_line.py file.fasta" )
