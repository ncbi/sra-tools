#!/opt/python-all/bin/python
from sam import *

def dump( acc ) :
    print "SEQ"
    vdb_dump( acc, "-C READ -l0" )
    print "PRIM"
    vdb_dump( acc, "-T PRIMARY_ALIGNMENT -C READ -l0" )
    print "SEC"
    vdb_dump( acc, "-T SECONDARY_ALIGNMENT -C READ -l0" )

REF   = "NC_011752.1"
ALIAS = "c1"
CSRA1 = "X.CSRA"
CSRA2 = "S.CSRA"

def load_sort( L ) :
    R1 = bam_load( L, CSRA1, "--make-spots-with-secondary -L 3 -E0 -Q0" )
    print "bam-load = %d"%( R1 )
    if R1 == 1 :
        R2 = sra_sort( CSRA1, CSRA2, "-vvvv" )
        print "sra-sort = %d"%( R2 )

def load_sort_print( L ) :
    load_sort( L )
    dump( CSRA1 )
    dump( CSRA2 )


def test1() :
    A1 = make_prim( "A1", 0, REF, ALIAS, 17000, 20, "60M" )
    A2 = make_prim( "A2", 0, REF, ALIAS, 12500, 20, "50M" )
    A1.pair_with( A2 )

    A3 = make_prim( "A3", 0, REF, ALIAS, 33000, 20, "60M" )
    U1 = make_unaligned( "U1", 0, "ACTTTAGTAAGGGGTTNN" )

    A4 = make_sec( "A4", 0, REF, ALIAS, 19000, 20, "60M" )
    A4.link_to( A1 )

    A5 = make_sec( "A5", 0, REF, ALIAS, 22000, 20, "30M" )
    
    load_sort_print( [ A1, A2, A3, A4, U1, A5 ] )

# the resulting X.CSRA and S.CSRA produce errors in seq_restore_read_impl2
def test2() :
    A1 = make_prim( "A1", 0, REF, ALIAS, 17000, 20, "60M" )
    A2 = make_sec( "A2", 0, REF, ALIAS, 12500, 20, "50M" )
    A1.pair_with( A2 )

    load_sort_print( [ A1, A2 ] )

# the resulting X.CSRA produces errors in seq_restore_read_impl2
# but S.CSRA sefaults in vdb-dump!
def test3() :
    A1 = make_prim( "A1", 0, REF, ALIAS, 1000, 20, "53M" )
    A2 = make_sec( "A2", 0, REF, ALIAS, 3500, 20, "50M" )
    A1.pair_with( A2 )

    A3 = make_sec( "A3", 0, REF, ALIAS, 6800, 20, "55M" )
    A3.flags |= FLAG_NEXT_UNMAPPED

    load_sort( [ A3, A1, A2 ] )

# the resulting X.CSRA produces errors in seq_restore_read_impl2
def test4() :
    A1 = make_prim( "A1", 0, REF, ALIAS, 1000, 20, "53M" )
    A2 = make_sec( "A2", 0, REF, ALIAS, 3500, 20, "50M" )
    A1.pair_with( A2 )

    A3 = make_sec( "A3", 0, REF, ALIAS, 6800, 20, "55M" )
    A3.flags |= FLAG_NEXT_UNMAPPED

    load_sort( [ A1, A2, A3 ] )

test3()