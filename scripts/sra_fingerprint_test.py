#!/usr/bin/env python3

from sra_fingerprint_module import *
from pathlib import Path

binpath = os . path . join( str( Path . home() ), "ncbi-outdir", "sra-tools", "linux",
                        "gcc", "x86_64", "dbg", "bin" )

def run_all_tests() :
    sra_tools = os . path . join( str( Path . home() ), "devel", "sra-tools" )
    acc = os . path . join( sra_tools, "test", "external", "sra-info", "input", "Test_Sra_Info_Fingerprint" )
    FingerprintTests( acc, binpath ) . runall()

def check_acc_type() :
    for acc in [ "SRR000001", "SRR341578", "SRR12873784" ] :
        acc1 = Process( [ "res.sh", acc ] ) . result() . strip()
        print( f"{acc1} --> { AccessionTypeOf( acc1 ) }" )

def try_to_insert() :
    acc = Process( [ "res.sh", "SRR12873784" ] ) . result()
    if acc :
        acc = acc . strip()
        if PrepareForFingerprintInsert( acc, "abc", "1-1000", binpath ) :
            print( "ok" )
        #fpi = FingerprintInserter( "abc" )
        #fpi . run()

if __name__ == "__main__":
    try_to_insert()
