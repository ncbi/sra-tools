#! /usr/bin/env python
import sys

def perform( inputfile ) :
    for line in inputfile :
        c = line.strip().split( '\t' )
        print( c[ 1 ] )

if __name__ == '__main__':
    if len( sys.argv ) > 1 :
        perform( open( sys.argv[ 1 ], "r" ) )
    else :
        perform( sys.stdin )
