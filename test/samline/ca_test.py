from sam import *

def dump( acc ) :
    print "SEQ"
    vdb_dump( acc, "-C READ -l0" )
    print "PRIM"
    vdb_dump( acc, "-T PRIMARY_ALIGNMENT -C READ -l0" )
    print "SEC"
    vdb_dump( acc, "-T SECONDARY_ALIGNMENT -C READ -l0" )

def test1() :
    REF  = "NC_011752.1"
    CSRA1= "X.CSRA"
    CSRA2= "S.CSRA"
    
    A1 = make_prim( "A1", 0, REF, "c1", 17000, 20, "60M" )
    A2 = make_prim( "A2", 0, REF, "c1", 12500, 20, "50M" )
    A1.pair_with( A2 )

    A3 = make_prim( "A3", 0, REF, "c1", 33000, 20, "60M" )
    U1 = make_unaligned( "U1", 0, "ACTTTAGTAAGGGGTTNN" )

    A4 = make_sec( "A4", 0, REF, "c1", 19000, 20, "60M" )
    A4.link_to( A1 )

    A5 = make_sec( "A5", 0, REF, "c1", 22000, 20, "30M" )
    
    L = [ A1, A2, A3, A4, U1, A5 ]
    print_sam( L )
    
    R1 = bam_load( L, CSRA1, "--make-spots-with-secondary -L 3 -E0 -Q0" )
    print "bam-load = %d"%( R1 )

    if R1 == 1 :
        R2 = sra_sort( CSRA1, CSRA2 )
        print "sra-sort = %d"%( R2 )

    dump( CSRA1 )
    dump( CSRA2 )

def test2() :
    REF  = "NC_011752.1"
    CSRA1= "X.CSRA"
    CSRA2= "S.CSRA"
    
    A1 = make_prim( "A1", 0, REF, "c1", 17000, 20, "60M" )
    A2 = make_sec( "A2", 0, REF, "c1", 12500, 20, "50M" )
    A1.pair_with( A2 )

    L = [ A1, A2 ]
    print_sam( L )
    
    R1 = bam_load( L, CSRA1, "--make-spots-with-secondary -L 3 -E0 -Q0" )
    print "bam-load = %d"%( R1 )

    if R1 == 1 :
        R2 = sra_sort( CSRA1, CSRA2 )
        print "sra-sort = %d"%( R2 )

    dump( CSRA1 )
    dump( CSRA2 )
    
test2()