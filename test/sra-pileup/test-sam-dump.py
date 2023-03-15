#!/usr/bin/env python

import sys
import argparse
import subprocess
import os
import sqlite3
import csv

def db_name( acc : str ) -> str :
    return f"{acc}_test.db"

def sam_dump_exe( exe_path : str ) -> str :
    return os.path.join( exe_path, "sam-dump" )

def vdb_dump_exe( exe_path : str ) -> str :
    return os.path.join( exe_path, "vdb-dump" )

def extract_xi( arr ) :
    for a in arr :
        if a.startswith( "XI:i:" ) :
            return a[ 5: ]
    return "0"
    
def each_line_in_stdout( process ) :
    while True:
        output = process.stdout.readline()
        if output :
            yield output.strip().decode()
        if process.poll() is not None:
            break
    #get what is still stuck in process.stdout
    while True:
        output = process.stdout.readline()
        if output :
            yield output.strip().decode()
        else :
            break

def sqlite3_row_gen( con, stm : str, batchsize : int ) :
    cur = con.cursor()
    cur.execute( stm )    
    while True:
        rows = cur.fetchmany( batchsize )
        if len( rows ) < 1 :
            break
        for row in rows :
            yield row
    cur.close()

CTABSAM="""create table if not exists SAM (
ROW_ID INT, QNAME TEXT, FLAGS INT, REF TEXT, POS INT,
MAPQ INT,CIGAR TEXT, MREF TEXT, MPOS INT, TLEN INT, SEQ TEXT, QUAL TEXT ); """

CTABPRIM="""create table if not exists PRIM (
ROW_ID INT, QNAME TEXT, FLAGS INT, REF TEXT, POS INT,
MAPQ INT,CIGAR TEXT, MREF TEXT, MPOS INT, TLEN INT, SEQ TEXT, QUAL TEXT ); """

def create_tables( acc : str, con ) :
    cur = con.cursor()
    cur.execute( CTABSAM )
    cur.execute( CTABPRIM )
    con.commit()

def extract_xi( arr ) :
    for a in arr :
        if a.startswith( "XI:i:" ) :
            return a[ 5: ]
    return "0"

INS_SAM="""insert into SAM
( ROW_ID, QNAME, FLAGS, REF, POS, MAPQ, CIGAR, MREF, MPOS, TLEN, SEQ, QUAL )
values ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? ); """

def start_process( cmd ) :
    try:
        process = subprocess.Popen( cmd, shell=False, stdout=subprocess.PIPE )
    except:
        print("ERROR {} while running {}".format( sys.exc_info()[1], cmd ) )
        process = None
    return process
    
def load_sam_dump_output( exe_path : str, acc : str, con ) -> ( bool, int ) :
    print( "running: sam-dump" )
    process = start_process( [ sam_dump_exe( exe_path ), acc, "-n", "--primary", "--XI" ] )
    if process is None :
        return ( False, 0 )
    n = 0
    cur = con.cursor()
    for line in each_line_in_stdout( process ) :
        if not line.startswith( '@' ) :
            arr = line.split()
            arr.insert( 0, extract_xi( arr ) )
            cur.execute( INS_SAM, arr[ :12 ] )
            n += 1
            if n % 100 == 0 :
                con.commit()
    cur.close()
    con.commit()
    print( f"{n} rows inserted from sam-dump" )
    return ( True, n )

PRIM_COLS="ALIGN_ID,SEQ_SPOT_ID,SAM_FLAGS,REF_NAME,REF_POS,MAPQ,CIGAR_SHORT,MATE_REF_NAME,MATE_REF_POS,TEMPLATE_LEN,READ,SAM_QUALITY"

INS_PRIM="""insert into PRIM
( ROW_ID, QNAME, FLAGS, REF, POS, MAPQ, CIGAR, MREF, MPOS, TLEN, SEQ, QUAL )
values ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? ); """

def correct_prim_array( arr ) :
    refpos = int( arr[ 4 ] ) + 1
    arr[ 4 ] = f"{refpos}"
    if arr[ 7 ] == '' :
        arr[ 7 ] = '*'
    if arr[ 8 ] == '' :
        arr[ 8 ] = '0'
    
def load_vdb_dump_output( exe_path : str, acc : str, con ) -> ( bool, int ):
    print( "running: vdb-dump" )
    process = start_process( [ vdb_dump_exe( exe_path ), acc, "-T", "PRIM", "-f", "csv", "-C", PRIM_COLS ] )
    if process is None :
        return ( False, 0 )
    n = 0    
    cur = con.cursor()
    for line in each_line_in_stdout( process ) :
        arr = line.split( ',' )
        if len( arr ) != 12 :
            reader = csv.reader( [ line ] )
            arr2 = list( reader )
            arr=arr2[ 0 ]
        if len( arr ) != 12 :        
            print( f"problem in line #{n}: {arr}" )
        else :
            correct_prim_array( arr )
            cur.execute( INS_PRIM, arr )
            if n % 100 == 0 :
                con.commit()
        n += 1
    cur.close()
    con.commit()
    print( f"{n} rows inserted from vdb-dump" )
    return ( True, n )

def compare_sam_vs_vdb( con ) -> bool:
    print( "comparing: sam-dump vs. vdb-dump" )
    sam_gen = sqlite3_row_gen( con, "select * from SAM;", 100 )
    prim_gen = sqlite3_row_gen( con, "select * from PRIM;", 100 )
    res = True
    n = 0
    while True :
        try :
            sam_row = sam_gen.__next__()
            sam_stop = False
        except :
            sam_stop = True
        try :
            prim_row = prim_gen.__next__()
            prim_stop = False
        except :
            prim_stop = True
        if sam_stop and prim_stop :
            break
        if sam_stop or prim_stop :
            res = False
            break
        if sam_row != prim_row :
            res = False
            break
        n += 1
    print( f"{n} rows compared" )
    return res

# =================================================================
def run_test( exe_path : str, acc : str ) -> bool:
    print( f"path_to_executables: {exe_path}" )
    print( f"accession: {acc}" )
    os.remove( db_name( acc ) )
    con = sqlite3.connect( db_name( acc ) )
    create_tables( acc, con )
    res, sam_count = load_sam_dump_output( exe_path, acc, con )
    if res :
        res, vdb_count = load_vdb_dump_output( exe_path, acc, con )
    res = sam_count == vdb_count
    if res :
        res = compare_sam_vs_vdb( con )
    con.close()
    return res
# =================================================================

# --- some helpers to find the executables and pre-check an accession ---
def check_tool( tool : str ) -> bool :
    if not os.path.exists( tool ) :
        print( f"{tool} not found" )
        return False
    if not os.access( tool, os.X_OK ) :
        print( f"{tool} not executable" )
        return False
    return True

def check_executable_path( exe_path : str ) -> bool :
    b1 = check_tool( sam_dump_exe( exe_path ) )
    b2 = check_tool( vdb_dump_exe( exe_path ) )
    return b1 and b2

def check_accession( acc : str ) -> bool :
    srr = acc.startswith( "SRR" )
    drr = acc.startswith( "ERR" )
    err = acc.startswith( "DRR" )
    if srr or drr or err :
        if acc[ 3: ].isnumeric() :
            return True
    print( f"{acc} is an invalid accession" )
    return False

def search_for_executable( toolname : str ) -> str :
    for path in os.environ[ "PATH" ].split( os.pathsep ):
        exe_file = os.path.join( path, toolname )
        if os.access( exe_file, os.X_OK ) :
            return path
    print( "cannot find the tools in search-path" )
    return None

if __name__ == '__main__':
    parser = argparse.ArgumentParser( description = 'testing sam-dump' )
    parser.add_argument( '--acc', dest='acc', help='accession', required=True )
    parser.add_argument( '--exe', dest='exe', help='path to executables' )
    args = parser.parse_args()
    exe = args.exe
    if exe is None :
        exe = search_for_executable( "vdb-dump" )
    if not exe is None :
        if check_executable_path( exe ) :
            if check_accession( args.acc ) :
                if run_test( exe, args.acc ) :
                    print( "test: OK" )
                    sys.exit( os.EX_OK )
    print( "test: Failure" )
    sys.exit( 3 )
