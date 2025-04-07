#!/usr/bin/env python3

from sra_fingerprint import *
from pathlib import Path

class FingerprintTests :
    def __init__( self, acc, binpath = "" ) :
        self . acc = acc
        self . binpath = binpath
        self . tests = [ self . test1, self . test2, self . test3, self . test4,
                        self . test5, self . test6 ]

    def run( self, test_numbers ) :
        for n in test_numbers :
            if n <= len( self . tests ) :
                print( f"Test #{ n }" )
                self . tests[ n - 1 ]()

    # test if we can compute the fingerprint from the stored bases ( expensive! )
    def test1( self ) :
        print( Fingerprint . computeFromReadsOfAccession( self . acc ) )

    # test if we can extract the fingerprint from the metatdata
    def test2( self ) :
        print( Fingerprint . extractFromMetaOfAccession( self . acc ) )

    # test if we can compare the computed vs. the extracted fingerprint
    def test3( self ) :
        fp1 = Fingerprint . extractFromMetaOfAccession( self . acc )
        fp2 = Fingerprint . computeFromReadsOfAccession( self . acc )
        print( FingerprintComp( fp1, fp2 ) )

    # test if we can detect redaction of bases between 2 fingerprints
    def test4( self ) :
        fp1 = Fingerprint . extractFromMetaOfAccession( self . acc, self . binpath )
        fp2 = Fingerprint . computeFromReadsOfAccession( self . acc, self . binpath )
        fp2 . content[ 'A' ][ 1 ] = fp2 . content[ 'A' ][ 1 ] - 1
        fp2 . content[ 'N' ][ 1 ] = fp2 . content[ 'N' ][ 1 ] + 1
        fp2 . digest = Fingerprint . digest_from_string( fp2 . as_string() )
        fc = FingerprintComp( fp1, fp2 )
        print( fc )

    # test if we can safely compare a fingerprint against None
    def test5( self ) :
        fp1 = Fingerprint . extractFromMetaOfAccession( self . acc )
        fc = FingerprintComp( fp1, None )
        print( fc )

    # test if we can supply a path to the binaries instead of relying on them
    # beeing in the search-path
    def test6( self ) :
        print( Fingerprint . computeFromReadsOfAccession( self . acc, self . binpath ) )

# ---------------------------------------------------------------------------------------


if __name__ == "__main__":
    acc1 = "/home/wolfgang/devel/sra-tools/test/external/sra-info/input/Test_Sra_Info_Fingerprint"
    acc2 = "/mnt/data/SRA/ACC/TABLES_FLAT/SRR000001"
    binpath = os . path . join( str( Path . home() ), "ncbi-outdir", "sra-tools", "linux",
                           "gcc", "x86_64", "dbg", "bin" )

    t = FingerprintTests( acc1, binpath )
    t . run( list( range( 1, 7 ) ) )
