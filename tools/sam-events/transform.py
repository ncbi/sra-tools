#! /usr/bin/env python
import sys

def perform( inputfile ) :
    for line in inputfile :
        c = line.strip().split( '\t' )

        l = list()
        
        #   counters:
        #   IN      c[ 1 ]  c[ 2 ]  c[ 3 ]  c[ 4 ]
        #   OUT     2       3       1       0
        l.append( c[ 4 ] )
        l.append( c[ 3 ] )
        l.append( c[ 1 ] )
        l.append( c[ 2 ] )
        
        # c[ 5 ] = refname:pos,del,bases
        cc = c[ 5 ].split( ':' )
        l.append( cc[ 0 ] )
        l.append( cc[ 1 ] )
        l.append( cc[ 2 ] )
        if cc[ 3 ] == '-' :
            l.append( "0\t" )
        else :
            l.append( "%s"%len( cc[ 3 ] ) )
            l.append( cc[ 3 ] )
        print( '\t'.join( l ) )

if __name__ == '__main__':
    if len( sys.argv ) > 1 :
        perform( open( sys.argv[ 1 ], "r" ) )
    else :
        perform( sys.stdin )
