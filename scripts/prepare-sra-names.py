#!/usr/bin/env python3

import sys, argparse, os, re, glob
from argparse import RawTextHelpFormatter

#---------------------------------------------------------------------------
def is_accession( s : str ) -> bool :
    match = re.match( r'[DES]RR+([0-9])', s )
    return match is not None

#---------------------------------------------------------------------------
def rename( src : str, dst : str, verbose : bool ) :
    try :
        os.rename( src, dst )
        if verbose :
            print( "renamed: {} -> {}".format( src, dst ) )
    except Exception as e :
        print( "error {} while renaming {} -> {}".format( e, src, dst ) )

#---------------------------------------------------------------------------
def mkdir( name : str ) -> bool:
    try :
        os.makedirs( name )
        return True
    except Exception as e :
        print( "error {} while creating directory {}".format( e, name ) )
    return False

#---------------------------------------------------------------------------
def prepare_names( acc : str, verbose : bool ) :
    print( '{}'.format( acc ) )
    if os.sep in acc :
        print( "{} is a path - accession expected".format( acc ) )
    elif is_accession( acc ) :
        files = glob.glob( "{}*".format( acc ) )
        if len( files ) > 0 :
            b = os.path.isdir( acc )
            if not b :
                b = mkdir( acc )
            if b :
                for file in files :
                    rename( file, "{}{}{}".format( acc, os.sep, file ), verbose )
        else :
            if verbose :
                print( 'no files' )
    else :
        if verbose :
            print( "{} is not an accession".format( acc ) )

#---------------------------------------------------------------------------
def parse_cmdline() :
    desc="""
Rearrange run files in current directory
by creating accession directories and moving files there.

This script is intended to rearrange sra files
that were downloaded without prefetch.
Next step: run "fix-sra-names.py <accession> [<accession>]..."

Examples of directory with such files:
$ls GSE118828
SRR7725681.1 SRR7725682.1 SRR7725682.vdbcache.1 SRR7725683.1 SRR7725684.1
...

Script should be executed in directory that contains SRR* files.

Usage of prepare-sra-names.py:
$./prepare-sra-names.py SRR7725681 SRR7725682 SRR7725683 SRR7725684

The script will create a directory for accession
and move files related to this accession there.

After prepare-sra-names.py was executed -
run fix-sra-names.py from the same directory with the same argument list.
"""
    parser = argparse.ArgumentParser( description=desc, formatter_class=RawTextHelpFormatter )
    parser.add_argument( '--verbose', '-v', action='store_true',
        dest='verbose', default=False, help='print preparing messages' )
    parser.add_argument( 'accessions', metavar='accession', nargs='+',
        help='accessions to process' )
    return parser.parse_args()

#---------------------------------------------------------------------------
if __name__ == "__main__" :
    args = parse_cmdline()
    for acc in args.accessions :
        prepare_names( acc, args.verbose )
