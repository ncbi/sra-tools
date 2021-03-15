#!/usr/bin/env python3

import sys, argparse, os, glob
from argparse import RawTextHelpFormatter

#---------------------------------------------------------------------------
def str_to_int( s : str ) -> int :
    try :
        return int( s )
    except :
        return None

#---------------------------------------------------------------------------
def get_max_vers( dir : str, verbose : bool ) -> int:
    res = None
    for f in glob.glob( "{}{}{}.*".format( dir, os.sep, dir ) ) :
        fn, fext = os.path.splitext( f )
        num_ext = str_to_int( fext[ 1: ] )
        if num_ext is not None :
            if res is not None :
                res = max( res, num_ext )
            else :
                res = num_ext
    return res

#---------------------------------------------------------------------------
def rename( src : str, dst : str, verbose : bool ) :
    try :
        os.rename( src, dst )
        if verbose :
            print( "renamed: {} -> {}".format( src, dst ) )
    except Exception as e :
        print( "error {} while renaming {} -> {}".format( e, src, dst ) )

#---------------------------------------------------------------------------
def fix_names( dir : str, verbose : bool ) :
    if verbose :
        print( '\nfixing directory: "{}"'.format( dir ) )
    if os.path.isdir( dir ) :
        sra_data_file = "{}{}{}.sra".format( dir, os.sep, dir )
        if os.path.isfile( sra_data_file ) :
            if verbose :
                print( "{} found ... stop".format( sra_data_file ) )
        else :
            if verbose :
                print( "{} not found ... continue".format( sra_data_file ) )
            sra_cache_file = "{}{}{}.sra.vdbcache".format( dir, os.sep, dir )
            if os.path.isfile( sra_cache_file ) :
                if verbose :
                    print( "{} found ... stop".format( sra_cache_file ) )
            else :
                if verbose :
                    print( "{} not found ... continue".format( sra_cache_file ) )
                max_version = get_max_vers( dir, verbose )
                if verbose :
                    print( "max. version = {}".format( max_version ) )
                if max_version is not None :
                    src = "{}{}{}.{}".format( dir, os.sep, dir, max_version )
                    rename( src, sra_data_file, verbose )
                    src = "{}{}{}.vdbcache.{}".format( dir, os.sep, dir, max_version )
                    if os.path.isfile( src ) :
                        rename( src, sra_cache_file, verbose )
    else :
        if verbose :
            print( 'directory "{}" does not exist'.format( dir ) )

#---------------------------------------------------------------------------
def parse_cmdline() :
    desc="""
Rename names of run files in <accession directory> to sra format
to be recognized by SRA tools.

This script is intended to fix names of sra files
that were downloaded without prefetch to be recognized by SRA tools.

Examples of directory with such files:
$ls SRR8639211
SRR8639211.1

$ls SRR8639212
SRR8639212.1  SRR8639212.vdbcache.1

The Script should be executed
in the directory that contains directories SRR8639211 [and SRR8639212].

Usage of fix-sra-names.py:
$ls -d SRR8639211 SRR8639212
SRR8639211 SRR8639212
$./fix-sra-names.py SRR8639211 SRR8639212

After fix-sra-names.py was executed
you can run SRA tools from the same current directory
using accession as command line argument:
$ls SRR8639211
SRR8639211.1
$./fix-sra-names.py SRR8639211
$sam-dump SRR8639211'
   # Now sam-dump will find run files located in SRR8639211 and use them.
'N.B. Running "prefetch SRR8639211" is recommended before dumping SRR8639211.
"""
    parser = argparse.ArgumentParser( description=desc, formatter_class=RawTextHelpFormatter )
    parser.add_argument( '--verbose', '-v', action='store_true',
        dest='verbose', default=False, help='print moving of files')
    parser.add_argument( 'dirs', metavar='directory', nargs='+', help='directories to convert' )
    return parser.parse_args()

#---------------------------------------------------------------------------
if __name__ == "__main__" :
    args = parse_cmdline()
    for dir in args.dirs :
        fix_names( dir, args.verbose )
