#!/usr/bin/env python3

from sra_fingerprint_module import *
from pathlib import Path

binpath = os . path . join( str( Path . home() ), "ncbi-outdir", "sra-tools", "linux",
                        "gcc", "x86_64", "dbg", "bin" )

ACCSET = [ "SRR000001", "SRR341578", "SRR15233072" ]

def run_all_tests() :
    sra_tools = os . path . join( str( Path . home() ), "devel", "sra-tools" )
    acc = os . path . join( sra_tools, "test", "external", "sra-info", "input", "Test_Sra_Info_Fingerprint" )
    FingerprintTests( acc, binpath ) . runall()

def check_acc_type() :
    for acc in ACCSET :
        acc = Process( [ "res.sh", acc ] ) . result() . strip()
        print( f"{acc} --> { AccessionTypeOf( acc ) }" )

def try_to_insert() :
    acc = Process( [ "res.sh", ACCSET[ 2 ] ] ) . result()
    if acc :
        if PrepareForFingerprintInsert( acc . strip(), "abc" ) :
            FingerprintInserter( "abc" ) . run()

if __name__ == "__main__":
    run_all_tests()
    check_acc_type()
#    try_to_insert()
