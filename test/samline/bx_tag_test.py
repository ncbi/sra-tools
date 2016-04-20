#!/opt/python-all/bin/python
from sam import *

REF   = "NC_011752.1"
ALIAS = "c1"
CSRA1 = "X.CSRA"

def load( L ) :
    R1 = bam_load( L, CSRA1, "--make-spots-with-secondary -E0 -Q0" )
    print "bam-load = %d"%( R1 )

def load_and_print( L ) :
    load( L )
    sam_dump( CSRA1 )


def test1() :
    A1 = make_prim( "A1", 0, REF, ALIAS, 17000, 20, "60M" )
    A2 = make_prim( "A2", 0, REF, ALIAS, 12500, 20, "50M" )
    A1.pair_with( A2 )

    A1.set_tags( "BX:Z:i_am_a_BX_tag" )
    
    load_and_print( [ A1, A2 ] )


test1()
