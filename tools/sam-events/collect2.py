#! /usr/bin/env python
import sys


def collect( fn, nr ) :
    for line in open( fn, "r" ) :
        c = line.strip().split( '\t' )
        cn0  = int( c[ 0 ] )
        cn1  = int( c[ 1 ] )
        cn2  = int( c[ 2 ] )
        cn3  = int( c[ 3 ] )
        ref  = c[ 4 ]
        pos  = int( c[ 5 ] )
        dels = int( c[ 6 ] )
        ins  = int( c[ 7 ] )
        base = c[ 8 ] if len( c ) > 8 else ''
        print( "%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\t[%d]"%( ref, pos, dels, ins, base, cn0, cn1, cn2, cn3, nr ) )

if __name__ == '__main__':
    for x in range( 1, len( sys.argv ) ) :
        collect( sys.argv[ x ], x )
