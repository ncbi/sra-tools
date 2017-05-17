#! /usr/bin/env python
import sys

def perform( inputfile ) :
    for line in inputfile :
        c = line.strip().split( '\t' )
        phases = c[ 0 ]
        n = phases.count( 'P' ) + phases.count( 'N' )
        allele = c[ 1 ].replace( '-', ''  )
        for x in range( 0, n ) :
            print( allele )

if __name__ == '__main__':
    if len( sys.argv ) > 1 :
        perform( open( sys.argv[ 1 ], "r" ) )
    else :
        perform( sys.stdin )
