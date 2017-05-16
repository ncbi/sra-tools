#! /usr/bin/env python
import sys

# the global dictionary, containing an entry for each reference
d = dict()

n_equal = 0
n_different = 0
n_only_A = 0
n_only_B = 0

class Counters :
    def __init__( self, c, nr ) :
        self.nr = nr
        self.c = list( map( ( lambda x : int( x ) ), c[ 0 : 4 ]  ) )

    def printout( self ) :
        if self.nr == 1 :
            return "\t\tA = %s"%self.c
        else :
            return "\t\tB = %s"%self.c

    def equal( self, other ) :
        return self.c[0]==other.c[0] and self.c[1]==other.c[1] and self.c[2]==other.c[2] and self.c[3]==other.c[3]


class Allele :
    def __init__( self, id, c, nr ) :
        self.name = id
        self.count = 1
        self.pos = int( c[ 5 ] )
        self.a = { nr : Counters( c, nr ) }

    def collect( self, c, nr ) :
        self.count += 1
        if nr in self.a :
            print( "error!" )
        else :
            self.a[ nr ] = Counters( c, nr )

    def print_counters( self ) :
        print( "\t%s"%( self.name ) )
        for k in self.a :
            print( self.a[ k ].printout() )

    def cmp_and_print_counters( self ) :
        a = self.a[ 1 ]
        b = self.a[ 2 ]
        equal = a.equal( b )
        if equal :
            print( "\t%s ( EQUAL! )"%self.name )
        else :
            print( "\t%s"%self.name )
        print( a.printout() )
        print( b.printout() )
        return equal

    def has_1_and_2( self ) :
        return ( 1 in self.a ) and ( 2 in self.a )

    def has_1( self ) :
        return ( 1 in self.a ) and not( 2 in self.a )

    def has_2( self ) :
        return ( 2 in self.a ) and not( 1 in self.a )


def c_2_id( c ) :
    if len( c ) > 8 :
        return "%s:%s:%s:%s"%( c[ 4 ], c[ 5 ], c[ 6 ], c[ 8 ] )
    else :
        return "%s:%s:%s"%( c[ 4 ], c[ 5 ], c[ 6 ] )


class RefObj :
    def __init__( self, c, nr ) :
        self.name = c[ 4 ]
        self.a = { id : Allele( c_2_id( c ), c, nr ) }
        
    def collect( self, c, nr ) :
        id = c_2_id( c )
        if id in self.a :
            self.a[ id ].collect( c, nr )
        else :
            self.a[ id ] = Allele( id, c, nr )

    def printout( self, nr, limit ) :
        print( "%s :"%( self.name ) )
        for k in self.a :
            self.a[ k ].printout( nr, limit )

    def printout_1_and_2( self ) :
        n_eq = 0
        n_neq = 0
        #filter alleles that have both sources
        l = list()
        for k in self.a :
            if self.a[ k ].has_1_and_2() :
                l.append( self.a[ k ] )
        #sort list by position
        l.sort( key = lambda x : x.pos )
        print( "%s :"%( self.name ) )
        for a in l :
            if a.cmp_and_print_counters() :
                n_eq += 1
            else :
                n_neq += 1
        return ( n_eq, n_neq )

    def printout_1( self ) :
        #filter alleles that have only source A
        l = list()
        for k in self.a :
            if self.a[ k ].has_1() :
                l.append( self.a[ k ] )
        #sort list by position
        l.sort( key = lambda x : x.pos )
        print( "%s :"%( self.name ) )
        for a in l :
            a.print_counters()
        return len( l )


    def printout_2( self ) :
        #filter alleles that have only source B
        l = list()
        for k in self.a :
            if self.a[ k ].has_2() :
                l.append( self.a[ k ] )
        #sort list by position
        l.sort( key = lambda x : x.pos )
        print( "%s :"%( self.name ) )
        for a in l :
            a.print_counters()
        return len( l )


def perform( fn, nr ) :
    if nr == 1 :
        print( "reading '%s' as input A"% fn )
    else :
        print( "reading '%s' as input B"% fn )    

    for line in open( fn, "r" ) :
        c = line.strip().split( '\t' )
        ref = c[ 4 ]
        if ref in d :
            d[ ref ].collect( c, nr )
        else :
            d[ ref ] = RefObj( c, nr )


if __name__ == '__main__':
    for x in range( 1, len( sys.argv ) ) :
        perform( sys.argv[ x ], x )
        
    for k in d :
        n_eq, n_neq = d[ k ].printout_1_and_2()
        n_equal += n_eq
        n_different += n_neq
        
    print( '-'*60 )
    
    for k in d :
        n_only_B += d[ k ].printout_2()
        
    print( '-'*60 )
    
    for k in d :
        n_only_A += d[ k ].printout_1()

    print( '-'*60 )
    print( "Alleles EQUAL  : %d"%n_equal )
    print( "Alleles differ : %d"%n_different )
    print( "Alleles only A : %d"%n_only_A )
    print( "Alleles only B : %d"%n_only_B )
        