#!/usr/bin/env python3

import sys, argparse, os, sqlite3, json, subprocess
from argparse import RawTextHelpFormatter
from time import perf_counter
from shutil import which

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

def RunWithTime( cmd : str ) -> float :
    t_start = perf_counter()
    print( cmd )
    os.system( cmd )
    return ( perf_counter() - t_start )

def du( path : str ) -> int:
    return int( subprocess.check_output(['du','-s', path]).split()[0].decode('utf-8') ) * 1024

def is_cSRA( path : str ) -> bool :
    s = []
    a = subprocess.check_output( [ 'vdb-dump', '-E', path ] ).decode( 'utf-8' ).split( '\n' )
    for line in a[ 1: ] :
        d = line.split()
        if len( d ) > 2 :
            s.append( d[ 2 ] )
    return 'REFERENCE' in s and 'PRIMARY_ALIGNMENT' in s and 'SEQUENCE' in s

def tool_available( name : str ) -> bool:
    return which( name ) is not None

def ProcessFlat( acc : str, jobsJson, db : str, redo :  bool, tag : str, data_dir : str ) -> bool:
    print( 'fastq-extraction from flat SRA-accession' )
    acc_location = os.path.join( data_dir, acc )
    if not os.path.exists( f"{acc_location}.fastq" ) or redo :
        args = f"{acc_location} -O {data_dir} --defline-seq '>$ac.$sn.$ri' --defline-qual '+$ac.$sn.$ri' --split-spot"
        cmd = f"fastq-dump {args}"
        print( cmd )
        os.system( cmd )
    else:
        print( f"{acc_location}.fastq exists")

    # update the schema (TBD)
    loader = jobsJson[ "loader" ]
    transform_array = jobsJson[ "transform" ]
    # now iterate over the different
    for transform in transform_array :
        transform_tag = TransformSchema( loader, transform )

        print( f"transform: {transform_tag}")
        print( f"tag: {tag}")
        # load, measure
        args = f"{acc_location}.fastq --quality PHRED_33 --output {acc_location}.new"
        cmd = f"{loader} {args}"
        time = RunWithTime( cmd )

        oldSize = du( f'{acc_location}' )
        print( f"old size: {oldSize}" )
        print( f"load time: {time:.2f}s" )
        newSize = du( f'{acc_location}.new' )
        improved = ( newSize * 100 ) / oldSize
        print( f"new size: {newSize} (changed to {improved} %)" )
        pergb = ( time * 1024 * 1024 * 1024 ) / newSize
        UpdateDatabase( db, ( tag, loader, args, transform_tag, name, time, oldSize, newSize, improved, pergb ) )
    return True
    
def ProcesscSRA( acc : str, jobsJson, db : str, redo :  bool, tag : str, data_dir : str ) -> bool:
    print( 'sam-extraction from cSRA-accession' )
    acc_location = os.path.join( data_dir, acc )
    if not os.path.exists( f"{acc_location}.sam" ) or redo :
        cmd = f"sam-dump --output-file {acc_location}.sam -u {acc_location}"
        print( cmd )
        os.system( cmd )
    return True

def ProcessAccession( acc : str, jobsJson, db : str, redo :  bool, tag : str, data_dir : str ) -> bool:
    print( "="*60 )
    print( f"ProcessAccession( {acc} )")
    #print( f"Jobs:( {jobsJson} )")
    acc_location = os.path.join( data_dir, acc )

    # prefetch if required
    if not os.path.exists( acc_location ) :
        cmd = f"prefetch {acc} -O {data_dir}"
        print( cmd )
        os.system( cmd )
    else:
        print( f"{acc_location} exists (already prefetched)")

    # fix eventually misplaced reference-files
    if os.path.exists( acc_location ) and os.path.exists( acc ) :
        print( f"fixing misplaced references for {acc} ..." )
        os.system( f"mv {acc}/* {acc_location}" )
        os.system( f"rm -rf {acc}" )
        
    # detect if aligned or not-aligned
    if is_cSRA( acc_location ) :
        res = ProcesscSRA( acc, jobsJson, db, redo, tag, data_dir )
    else :
        res = ProcessFlat( acc, jobsJson, db, redo, tag, data_dir )        
    print("")
    return res

#---------------------------------------------------------------------------
if __name__ == "__main__" :
    parser = argparse.ArgumentParser( description="", formatter_class=RawTextHelpFormatter )
    parser.add_argument( '--accessions', '-a', dest='accessions', default='acc.txt', help='file containing list of accessions' )
    parser.add_argument( '--tag', '-t', dest='tag', default='', help='tag the run' )
    parser.add_argument( '--jobs', '-j', dest='jobs', default='jobs.json', help='Json file describing the set of jobs' )
    parser.add_argument( '--output', '-o', dest='output', default='out.db', help='SQLite database to write the results to (will be created if necessary)' )
    parser.add_argument( '--redo', '-r', dest='redo', action='store_true', default=False, help='forced re-creation of fastq/sam-file' )
    parser.add_argument( '--data-dir', '-d', dest='data_dir', default='./data', help='data-directory' )

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

    for tool in [ 'vdb-dump', 'fastq-dump', 'latf-load', 'bam-load' ] :
        if not tool_available( tool ) :
            print( f"cannot find the tool '{tool}'" )
            sys.exit( 4 )

    jobs = {}
    with open( args.jobs, 'r' ) as jobsFile:
        jobs = json.load( jobsFile )

    # create data-dir if it does not exist...
    if not os.path.exists( args.data_dir ):
        os.makedirs( args.data_dir )

    with open( args.accessions, 'r' ) as accFile:
        accs = accFile.readlines()
        for acc in accs:
            if not acc.startswith( '#' ) :
                ProcessAccession( acc.strip(), jobs, args.output, args.redo, args.tag, args.data_dir )
