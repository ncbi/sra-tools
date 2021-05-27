#!/usr/bin/env python3

import sys, argparse, os
from argparse import RawTextHelpFormatter
import sqlite3
import json
from time import perf_counter
import subprocess

DataDir = "./data"

#increased default-blob-size from 131072 ( 128k ) to 8388608 (8MB) 
LoaderSettings = {
    "latf-load" :
        (
            "latf-load.vschema.templ",
            "latf-load.vschema",
            {
                "${ENCODE-READ}" : "INSDC:2na:packed",
                "${ENCODE-ALTREAD}" : "< INSDC:4na:bin > zip_encoding ",
                "${ENCODE-ORIGINAL_QUALITY}" : "< INSDC:quality:phred > zip_encoding",
                "${ENCODE-RAW_NAME}" : "< ascii > zip_encoding",
                "${SEQ_BLOB_SIZE}": "8388608"
            }
        )
    }

def TransformSchema( loader, transform ) -> str:
    settings = LoaderSettings[ loader ]
    if settings == None:
        raise "bad loader: " + loader
    tag = ""
    source = settings[0]
    target = settings[1]
    transforms = settings[2]
    with open( source, 'r' ) as src:
        schemaText = src.read()
        for t in transforms.keys():
            override = transform.get( t )
            if override == None:
                override = transforms[ t ]
            else :
                tag = tag + f"{t}->{override}."
            schemaText = schemaText.replace( t, override )

        with open( target, 'w' ) as tgt:
            tgt.write( schemaText )
    return tag

def CreateDatabase( filename : str ) -> bool:
    try:
        conn = sqlite3.connect(filename)

        stmt="""CREATE TABLE IF NOT EXISTS
            'log' (
                'id' INTEGER PRIMARY KEY,
                'ts' DATETIME NOT NULL DEFAULT ( datetime( 'now', 'localtime' ) ) ,
                'tag' TEXT, /* to tag what was done */
                'tool' TEXT, /* loader, dumper */
                'toolargs' TEXT, /* full cmdline */
                'schema' TEXT, /* the schema change: TBD */
                'acc' TEXT,
                'runtime' REAL,
                'orgsize' INTEGER,
                'ressize' INTEGER,
                'improved' INTEGER,
                'pergb' REAL ) """
        conn.execute( stmt )
        conn.close()
        return True
    except:
        print( f"CreateDatabase failed: {sys.exc_info()[0]}", file=sys.stderr )
        return False

def UpdateDatabase( filename: str, data ) -> bool:
    try:
        conn = sqlite3.connect(filename)
        stmt="""INSERT INTO 'log'
            ( tag, tool, toolargs, schema, acc, runtime, orgsize, ressize, improved, pergb )
            VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )"""
        conn.execute( stmt, data )
        conn.commit()
        conn.close()
        return True
    except e:
        print( f"UpdateDatabase failed: {e}", file=sys.stderr )
        return False

def RunWithTime( cmd ) -> float :
    t_start = perf_counter()
    print( cmd )
    os.system( cmd )
    return ( perf_counter() - t_start )

def du(path) -> int:
    return int( subprocess.check_output(['du','-s', path]).split()[0].decode('utf-8') ) * 1024

def ProcessAccession( name : str, jobsJson, db : str, redo_fastq :  bool, tag : str ) -> bool:
    print( f"ProcessAccession({name})")
    print( f"Jobs:({jobsJson})")

    # prefetch if required
    if not os.path.exists( os.path.join( DataDir, name ) ) :
        cmd = f"prefetch {name} -o {DataDir}/{name}"
        print( cmd )
        os.system( cmd )
    else:
        print( f"{DataDir}/{name} exists")

    # dump if required
    # TBD: split spots?
    if not os.path.exists( os.path.join( DataDir, name+".fastq" ) ) or redo_fastq :
        #fastq-dump data/SRR13340289 -X 5 --defline-seq '>$ac.$sn.$ri' --defline-qual '+$ac.$sn.$ri' --split-spot -Z
        #cmd = f"fastq-dump {DataDir}/{name} --split-files -O {DataDir}"
        args = f"{DataDir}/{name} -O {DataDir} --defline-seq '>$ac.$sn.$ri' --defline-qual '+$ac.$sn.$ri' --split-spot"
        cmd = f"fastq-dump {args}"
        print( cmd )
        os.system( cmd )
    else:
        print( f"{DataDir}/{name}.fastq exists")

    # update the schema (TBD)
    loader = jobsJson["loader"]
    transform_array = jobsJson["transform"]
    # now iterate over the different
    for transform in transform_array :
        transform_tag = TransformSchema( loader, transform )

        print( f"transform: {transform_tag}")
        print( f"tag: {tag}")
        # load, measure
        args = f"{DataDir}/{name}.fastq --quality PHRED_33 --output {DataDir}/{name}.new"
        cmd = f"{loader} {args}"
        time = RunWithTime( cmd )

        oldSize = du(f'{DataDir}/{name}')
        print( f"old size: {oldSize}")
        print( f"load time: {time:.2f}s" )
        newSize = du(f'{DataDir}/{name}.new')
        improved = ( newSize * 100 ) / oldSize
        print( f"new size: {newSize} (changed to {improved} %)")
        pergb = ( time * 1024 * 1024 * 1024 ) / newSize
        UpdateDatabase( db, ( tag, loader, args, transform_tag, name, time, oldSize, newSize, improved, pergb ) )

    print("")
    return True

#---------------------------------------------------------------------------
if __name__ == "__main__" :
    parser = argparse.ArgumentParser( description="", formatter_class=RawTextHelpFormatter )
    parser.add_argument( '--accessions', '-a', dest='accessions', default='acc.txt', help='file containing list of accessions')
    parser.add_argument( '--tag', '-t', dest='tag', default='', help='tag the run')
    parser.add_argument( '--jobs', '-j', dest='jobs', default='jobs.json', help='Json file describing the set of jobs')
    parser.add_argument( '--output', '-o', dest='output', default='out.db', help='SQLite database to write the results to (will be created if necessary)')
    parser.add_argument( '--redo-fastq', '-r', dest='redo_fastq', action='store_true', default=False, help='forced re-creation of fastq-file')

    args = parser.parse_args()
    print( f"accessions={args.accessions}" )
    print( f"jobs={args.jobs}" )
    print( f"output={args.output}" )

    if not os.path.isfile( args.accessions ):
        print( f"accessions file {args.accessions} does not exist", file=sys.stderr )
        sys.exit( 1 )
    if not os.path.isfile( args.jobs ):
        print( f"accessions file {args.jobs} does not exist", file=sys.stderr )
        sys.exit( 2 )
    if not os.path.isfile( args.output ):
        print( f"creating output file {args.output}" )
        if not CreateDatabase( args.output ):
            sys.exit( 3 )

    jobs = {}
    with open( args.jobs, 'r' ) as jobsFile:
        jobs = json.load( jobsFile )

    if not os.path.exists( DataDir ):
        os.makedirs( DataDir )

    with open( args.accessions, 'r' ) as accFile:
        accs = accFile.readlines()
        for acc in accs:
            if not acc.startswith( '#' ) :
                ProcessAccession( acc.strip(), jobs, args.output, args.redo_fastq, args.tag )
