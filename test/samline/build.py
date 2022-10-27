#!/bin/env python
import sys, sam

def main() -> int :
    print( "building list of alignments" )
    builder = sam.SamBuilder()
    builder.read()  # from stdin ...
    L = builder.to_list()
    print( "saving list of alignments" )
    sam.save_sam( L, "SAM1.sam" )
    sam.save_config( L, "SAM1.config" )
    print( "loading SAM-file" )
    res = sam.bam_load( L, "SAM1", "--make-spots-with-secondary -L 3 -E0 -Q0" )
    print( f"bam-load: {res}")
    if res == 1 :
        print( "sorting loaded SAM-file" )
        sam.rm_dir( "SAM2" )
        res = sam.sra_sort( "SAM1", "SAM2" )
        print( f"sra-sort: {res}")
        if res == 1 :
            print( f"print stats for SAM1:" )
            sam.sra_stat( "SAM1", "SAM1.stats"  )
            print( f"print stats for SAM2:" )
            sam.sra_stat( "SAM2", "SAM2.stats"  )
                
if __name__ == '__main__':
    sys.exit( main() )
