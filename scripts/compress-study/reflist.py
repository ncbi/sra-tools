#!/usr/bin/env python3

import sys, argparse, os, subprocess
from argparse import RawTextHelpFormatter
from shutil import which

def tool_available( name : str ) -> bool:
    return which( name ) is not None

def ProcessAccession( acc : str, data_dir : str ) :
    acc_location = os.path.join( data_dir, acc )
    cmd = f"sra-pileup {acc_location} --function ref"
    a = subprocess.check_output( [ 'sra-pileup', acc_location, '--function', 'ref' ] ).decode( 'utf-8' ).split( '\n' )
    for line in a :
        s = line.strip()
        if s and not s.startswith( 'resolved into' ) and not s.startswith( 'this object uses' ) :
            b = s.split( '=' )
            if b[ 0 ].strip().endswith( 'Extern' ) :
                print( b[ 1 ].strip() )
    return acc

#---------------------------------------------------------------------------
if __name__ == "__main__" :
    parser = argparse.ArgumentParser( description="", formatter_class=RawTextHelpFormatter )
    parser.add_argument( '--accessions', '-a', dest='accessions', default='acc.txt', help='file containing list of accessions' )
    parser.add_argument( '--data-dir', '-d', dest='data_dir', default='./data', help='data-directory' )
    
    args = parser.parse_args()
    print( f"accessions = {args.accessions}" )
    print( f"data-dirs  = {args.data_dir}" )

    if not os.path.isfile( args.accessions ):
        print( f"accessions file {args.accessions} does not exist", file=sys.stderr )
        sys.exit( 1 )

    if not os.path.exists( args.data_dir ):
        print( f"data-dir {args.data_dir} does not exist", file=sys.stderr )
        sys.exit( 2 )

    if not tool_available( 'sra-pileup' ) :
        print( f"cannot find the tool 'sra-pileup'" )
        sys.exit( 3 )

    results = []
    with open( args.accessions, 'r' ) as accFile:
        for acc in accFile :
            if not acc.startswith( '#' ) :
                s = ProcessAccession( acc.strip(), args.data_dir )
                results.append( s )

    with open( f"{args.accessions}.refs", 'w' ) as f :
        for line in results :
            f.write( f"{line}\n" )
