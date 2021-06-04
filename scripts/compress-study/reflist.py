#!/usr/bin/env python3

import sys, argparse, os, subprocess, queue, threading
from argparse import RawTextHelpFormatter
from shutil import which

def tool_available( name : str ) -> bool :
    return which( name ) is not None

def get_acc_list( filename : str ) -> list :
    res = []
    with open( filename, 'r' ) as f:
        for acc in f :
            accs = acc.strip()
            if accs and not accs.startswith( '#' ) :
                res.append( accs )
    return res

def write_results( results : dict, filename : str ) :
    with open( filename, 'w' ) as f :
        for key, value in results.items() :
            f.write( f"{key}: ext={value[0]} int={value[1]}\n" )

def write_ext_results( results : dict, filename : str ) :
    with open( filename, 'w' ) as f :
        for key, value in results.items() :
            if value[ 0 ] > 0 and value[ 1 ] == 0 :
                f.write( f"{key}: ext={value[0]} int={value[1]}\n" )
    
#---------------------------------------------------------------------------
def ProcessAccession( acc : str, data_dir : str ) :
    print( acc )
    acc_location = os.path.join( data_dir, acc )
    num_extern = 0
    num_intern = 0
    a = subprocess.check_output( [ 'sra-pileup', acc_location, '--function', 'ref' ] ).decode( 'utf-8' ).split( '\n' )
    for line in a :
        s = line.strip()
        if s and not s.startswith( 'resolved into' ) and not s.startswith( 'this object uses' ) :
            b = s.split( '=' )
            if b[ 0 ].strip().endswith( 'Extern' ) :
                if 'no' == b[ 1 ].strip() :
                    num_intern += 1
                else :
                    num_extern += 1
    print( f"{acc}: extern={num_extern}, intern={num_intern}" )
    return ( acc, num_extern, num_intern )

#---------------------------------------------------------------------------
class myThread ( threading.Thread ) :
    def __init__( self, Q_in, Q_out, data_dir ) :
        threading.Thread.__init__( self )
        self.Q_in = Q_in
        self.Q_out = Q_out
        self.data_dir = data_dir

    def run( self ):
        running = True
        while running :
            try :
                acc = self.Q_in.get_nowait()
                res = ProcessAccession( acc, self.data_dir )
                self.Q_out.put( res )
            except :
                running = False
        
def MultiThreaded( accessions : list, results : dict, data_dir : str, num_threads : int ) :
    ToDoQueue = queue.Queue( len( accessions ) )
    for acc in accessions :
        ToDoQueue.put( acc )
    ResultQueue = queue.Queue( len( accessions ) )
    threads = []
    for i in range( 0, num_threads ) :
        t = myThread( ToDoQueue, ResultQueue, data_dir )
        threads.append( t )        
        t.start()
    for t in threads:
        t.join()
    while not ResultQueue.empty() :
        r_acc, r_ext, r_int = ResultQueue.get()
        results[ r_acc ] = ( r_ext, r_int )

#---------------------------------------------------------------------------
if __name__ == "__main__" :
    parser = argparse.ArgumentParser( description="", formatter_class=RawTextHelpFormatter )
    parser.add_argument( '--accessions', '-a', dest='accessions', default='acc.txt', help='file containing list of accessions' )
    parser.add_argument( '--data-dir', '-d', dest='data_dir', default='./data', help='data-directory' )
    parser.add_argument( '--threads', '-t', dest='threads', type=int, default=1, help='how many threads' )
    
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

    accessions = get_acc_list( args.accessions )
    if len( accessions ) < 1 :
        print( f"no accessions in input-file'" )
        sys.exit( 4 )

    results = {}
    if args.threads == 1 :
        for acc in accessions :
            r_acc, r_ext, r_int = ProcessAccession( acc, args.data_dir )
            results[ r_acc ] = ( r_ext, r_int )
    else :
        MultiThreaded( accessions, results, args.data_dir, args.threads )

    #write_results( results, f"{args.accessions}.refs" )
    write_ext_results( results, f"{args.accessions}.ext" )
