#!/usr/bin/python3

def counting( file_list ):
    print( f"counting: {file_list}" )
    count_ACGT = 0
    count_N    = 0
    total      = 0
    aligned    = 0
    unaligned  = 0
    for filename in file_list :
        with open( filename, 'r') as file:
            for line in file:
                columns = line.strip().split()
                if len( columns ) > 10 :
                    read = columns[ 9 ]
                    count_ACGT += read.count( 'A' )
                    count_ACGT += read.count( 'C' )
                    count_ACGT += read.count( 'G' )
                    count_ACGT += read.count( 'T' )
                    count_N += read.count( 'N' )
                    total += len( read )
                    flags = int( columns[ 1 ] )
                    if ( flags & 0x004 ) == 0 :
                        unaligned += 1
                    else :
                        aligned += 1
    print( f"ACGT      = {count_ACGT}" )
    print( f"   N      = {count_N}" )
    print( f"total     = {total}" )
    print( f"aligned   = {aligned}")
    print( f"unaligned = {unaligned}")
    if total != count_ACGT + count_N :
        print( "total is not the sum of ACGT + N" )
    has_aligned_and_unaligned = ( aligned > 0 ) & ( unaligned > 0 )
    return ( count_ACGT, count_N, has_aligned_and_unaligned )

if __name__ == '__main__' :
    pass_file = "VDB-6398_pass.sam"
    fail_file = "VDB-6398_fail.sam"
    errors = 0

    count_ACGT, count_N, has_aligned_and_unaligned = counting( [ pass_file ] )
    if count_ACGT < count_N :
        print( "fail: ACGT < N" )
        errors += 1
    else :
        print( "  OK: ACGT > N" )
    if has_aligned_and_unaligned :
        print( "  OK: has aligned and unaliged" )
    else :
        print( "fail: does not have aligned and unaliged" )
        errors += 1
    print()

    count_ACGT, count_N, has_aligned_and_unaligned = counting( [ fail_file ] )
    if count_ACGT > count_N :
        print( "fail: ACGT > N" )
        errors += 1
    else :
        print( "  OK: ACGT < N" )
    if has_aligned_and_unaligned :
        print( "  OK: has aligned and unaliged" )
    else :
        print( "fail: does not have aligned and unaliged" )
        errors += 1
    print()

    count_ACGT, count_N, has_aligned_and_unaligned = counting( [ pass_file, fail_file ] )
    if count_ACGT > count_N :
        print( "fail: ACGT > N" )
        errors += 1
    else :
        print( "  OK: ACGT < N" )
    print()

    if errors > 0 :
        print( "some tests failed!" )
        exit( 1 )

    print( "success!")
    exit( 0 )
