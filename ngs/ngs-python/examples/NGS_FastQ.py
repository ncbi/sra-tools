#!/usr/bin/env python3

#===========================================================================
#
#                           PUBLIC DOMAIN NOTICE
#              National Center for Biotechnology Information
#
# This software/database is a "United States Government Work" under the
# terms of the United States Copyright Act.  It was written as part of
# the author's official duties as a United States Government employee and
# thus cannot be copyrighted.  This software/database is freely available
# to the public for use. The National Library of Medicine and the U.S.
# Government have not placed any restriction on its use or reproduction.
#
# Although all reasonable efforts have been taken to ensure the accuracy
# and reliability of the software and data, the NLM and the U.S.
# Government do not and cannot warrant the performance or results that
# may be obtained by using this software or data. The NLM and the U.S.
# Government disclaim all warranties, express or implied, including
# warranties of performance, merchantability or fitness for any particular
# purpose.
#
# Please cite the author in any work or product based on this material.
#
#===========================================================================
#

import argparse, random

from ngs import NGS
from ngs.ErrorMsg import ErrorMsg
from ngs.ReadCollection import ReadCollection
from ngs.Read import Read
from ngs.ReadIterator import ReadIterator

def printfastq( id, bases, qual ) :
    print( "@{}".format( id ) )
    print( "{}".format( bases ) )
    print( "+{}".format( id ) )
    print( "{}".format( qual ) )

def fastq( run, start : int, count : int ) :
    with run.getReadRange( start, count, Read.all ) as it:
        while it.nextRead() :
            yield ( it.getReadId(), it.getReadBases(), it.getReadQualities() )

def fastq_split( run, start : int, count : int ) :
    with run.getReadRange( start, count, Read.all ) as it:
        while it.nextRead() :
            while it.nextFragment():
                yield ( it.getFragmentId(), it.getFragmentBases(), it.getFragmentQualities() )

def random_ints( max : int, count : int ) :
    for x in range( 0, count ) :
        yield random.randint( 1, max )

def random_fastq( run, count : int ) :
    for r in random_ints( run.getReadCount(), count ) :
        for read in fastq( run, r, 1 ) :
            yield read

def random_fastq_split( run, count : int ) :
    for r in random_ints( run.getReadCount(), count ) :
        for read in fastq_split( run, r, 1 ) :
            yield read

def main() :
    parser = argparse.ArgumentParser( description='produce FastQ using NGS' )

    parser.add_argument( 'accession', default=None, type=str, help='accession to process' )
    parser.add_argument( '-s', '--start', default=1, type=int, help='first row to use' )
    parser.add_argument( '-n', '--count', default=10, type=int, help='number of rows to use' )
    parser.add_argument( '-p', '--split', default=False, action='store_true', help='split the READS' )
    parser.add_argument( '-r', '--random', default=False, action='store_true', help='get n random rows' )

    args = parser.parse_args()
    if args.accession == None :
        print( "accession missing!" )
    else :
        try :
            with NGS.openReadCollection( args.accession ) as run:
                if args.random :
                    if args.split :
                        src = random_fastq_split( run, args.count )
                    else :
                        src = random_fastq( run, args.count )
                else :
                    if args.split :
                        src = fastq_split( run, args.start, args.count )
                    else :
                        src = fastq( run, args.start, args.count )
                for read in src :
                    printfastq( *read )
        except ErrorMsg as e :
            print( "error: {}".format( e ) )

if __name__ == "__main__" :
    main()
