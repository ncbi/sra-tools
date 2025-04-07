#!/usr/bin/env python3

from sra_fingerprint_module import *
from pathlib import Path


if __name__ == "__main__":
    binpath = os . path . join( str( Path . home() ), "ncbi-outdir", "sra-tools", "linux",
                           "gcc", "x86_64", "dbg", "bin" )

    sra_tools = os . path . join( str( Path . home() ), "devel", "sra-tools" )

    acc1 = os . path . join( sra_tools, "test", "external", "sra-info", "input", "Test_Sra_Info_Fingerprint" )
    acc2 = "/mnt/data/SRA/ACC/TABLES_FLAT/SRR000001"

    t = FingerprintTests( acc1, binpath )
    t . run( list( range( 1, 7 ) ) )
