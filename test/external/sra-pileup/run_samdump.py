#!/usr/bin/env python

#   used to pre-process the output of sam-dump
#   to be inserted into a sqlite-db in csv-format

#   a) extract the value of the optional XI:i: field and prepend it
#   b) drop header-lines
#   c) drop optional fields to have a constant number of fields
#   d) create csv from tab-separated format

import sys
import argparse
import subprocess
import csv

def extract_xi( arr ) :
    for a in arr :
        if a.startswith( "XI:i:" ) :
            return a[ 5: ]
    return "0"
    
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
    writer = csv.writer( sys.stdout )
    for line in each_line_in_stdout( process ) :
        if not line.startswith( '@' ) :
            arr = line.split()
            arr.insert( 0, extract_xi( arr ) )
            writer.writerow( arr[ :12 ] )

if __name__ == '__main__':
    parser = argparse.ArgumentParser( description = 'run sam-dump' )
    parser.add_argument( '--exe', dest='exe', help='path to executable' )
    parser.add_argument( '--acc', dest='acc', help='accession' )
    parser.add_argument( '--opt', dest='opt', action="append", nargs="?", help='options' )

    args = parser.parse_args()
    if not args.exe is None :
        if not args.acc is None :
            cmd = [ args.exe, args.acc ]
            if not args.opt is None :
                for o in args.opt :
                    cmd.append( f"--{o}" )
            cmd.append( "--XI" )
            run( cmd )
