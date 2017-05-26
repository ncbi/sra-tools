#! /usr/bin/env python
import sys

class Record :
    def __init__( self, c ) :
        self.ref  = c[ 0 ]
        self.pos  = int( c[ 1 ] )
        self.dels = int( c[ 2 ] )
        self.ins  = int( c[ 3 ] ) 
        self.base = c[ 4 ]
        self.cn0 = int( c[ 5 ] )
        self.cn1 = int( c[ 6 ] )
        self.cn2 = int( c[ 7 ] )
        self.cn3 = int( c[ 8 ] )
        self.src = c[ 9 ]

    def to_str( self ) :
        return "%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\t%s"%( self.ref, self.pos, self.dels, self.ins, self.base, self.cn0, self.cn1, self.cn2, self.cn3, self.src )

    def equal( self, other ) :
        res = False
        if self.ref == other.ref :
            if self.pos == other.pos :
                if self.dels == other.dels :
                    if self.ins == other.ins :
                        if self.base == other.base :
                            if self.cn0 == other.cn0 :
                                if self.cn1 == other.cn1 :
                                    if self.cn2 == other.cn2 :
                                        if self.cn3 == other.cn3 :
                                            res = True
        return res
                
if __name__ == '__main__':
    equals = 0
    prev = None
    for line in sys.stdin :
        c = line.strip().split( '\t' )
        if prev == None :
            prev = Record( c )
        else :
            r = Record( c )
            if prev.equal( r ) :
                prev = None
                equals += 1
            else :
                print( prev.to_str() )
                prev = r
    print( '-'* 80 )
    print( "equals = %d"%equals )
    