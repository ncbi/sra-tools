#!/usr/bin/env python

#   used to split the output of 'vdb-dump SRRXXXXXX -I -C QUALITY -f csv
#   which will produce 1,"30, 31, 32 ..."

#   the output should be csv too, but 3 columns :
#       1,0,30
#       1,1,31
#       1,2,32
#       etc.
#   aka a tuple of ROW_ID,QUAL_VALUE_POS,QUAL_VALUE

import sys
import argparse
import subprocess
import csv

def each_event_in_line( line ) :
    a = line.split( "\t" )
    row_id = a[ 0 ]
    qualities = a[ 1 ].split( "," )
    q_count = len( qualities )
    idx = 0
    for quality in qualities :
        yield f"{row_id},{q_count},{idx},{quality.strip()}"
        idx += 1

def each_line_in_stdout( process ) :
    while True:
        output = process.stdout.readline()
        if process.poll() is not None:
            break
        if output :
            yield output.strip().decode()
    #get what is still stuck in process.stdout
    while True:
        output = process.stdout.readline()
        if output :
            yield output.strip().decode()
        else :
            break

def run( cmd ) :
    try:
        process = subprocess.Popen( cmd, shell=False, stdout=subprocess.PIPE )
    except:
        print("ERROR {} while running {}".format( sys.exc_info()[1], cmd ) )
        return None
    for line in each_line_in_stdout( process ) :
        for ev in each_event_in_line( line ) :
            print( ev )

if __name__ == '__main__':
    parser = argparse.ArgumentParser( description = 'run sam-dump' )
    parser.add_argument( '--exe', dest='exe', help='path to executable' )
    parser.add_argument( '--acc', dest='acc', help='accession' )

    args = parser.parse_args()
    if not args.exe is None :
        if not args.acc is None :
            cmd = [ args.exe, args.acc, "-I", "-ftab", "-CQUALITY" ]
            run( cmd )
