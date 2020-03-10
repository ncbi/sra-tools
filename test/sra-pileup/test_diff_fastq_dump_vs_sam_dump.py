#!/usr/bin/env python

import sys, getopt, subprocess, multiprocessing

def run( cmd, spots, q ) :
    p = subprocess.Popen( cmd, stdout = subprocess.PIPE )
    n = 0
    l = 0
    lines = None
    if spots != None :
        lines = spots * 4
    bases = None
    
    while True :
        line = p.stdout.readline().decode( 'ascii' ).strip()
        if line != '' :
            if l == 1 :
                bases = line
            elif l == 3 :
                q.put( ( bases, line ) )
            n += 1
            if lines != None :
                if n > lines :
                    p.kill()
                    break
            l += 1  
            if l > 3 :
                l = 0
        else :
            break
    q.put( None )

def fastq_dump_full( fastq_dump, acc, spots, q ) :
    print( "starting fastq-dump" )
    run( [ fastq_dump, '--split-3', '-Z', acc ], spots, q )
    print( "fastq-dump done" )

def sam_dump_full( sam_dump, acc, spots, q ) :
    print( "starting sam-dump" )
    run( [ sam_dump, '-u', '--fastq', acc ], spots, q )
    print( "sam-dump done" )

def handle_q( d, q ) :
    res = 0
    t = q.get()
    if t != None :
        res = 1
        ( bases, quality ) = t
        try :
            d[ bases ].append( quality )
        except :
            d[ bases ] = [ quality ]
    return res


def compare_lists( l1, l2 ) :
    cmn = 0
    rev = 0
    nom = 0
    for item in l1 :
        if item in l2 :
            cmn += 1
        elif item[::-1] in l2 :
            rev += 1
        else :
            nom += 1
    return ( cmn, rev, nom )


def reverse_compl( bases ) :
    res = ""
    for c in bases[::-1] :
        if c == 'A' :
            res += 'T'
        elif c == 'C' :
            res += 'G'
        elif c == 'G' :
            res += 'C'
        elif c == 'T' :
            res += 'A'
        else :
            res += c
    return res


class join_stats() :
    def __init__( self ) :
        self.n_matches = 0
        self.nr_matches = 0
        self.n_reversed = 0
        self.nr_reversed = 0
        self.n_no_match = 0
        self.nr_no_match = 0
        self.n_not_found = 0

    def add_match_res( self, m, r, n ) :
        self.n_matches  += m
        self.n_reversed += r
        self.n_no_match += n

    def add_rmatch_res( self, m, r, n ) :
        self.nr_matches  += m
        self.nr_reversed += r
        self.nr_no_match += n
    
    def __str__( self ) :
        res  = "matches     : %d\n" % self.n_matches
        res += "r-matches   : %d\n" % self.nr_matches
        res += "reversed    : %d\n" % self.n_reversed
        res += "r-reversed  : %d\n" % self.nr_reversed
        res += "no_match    : %d\n" % self.n_no_match
        res += "r-no_match  : %d\n" % self.nr_no_match
        res += "not_found   : %d" % self.n_not_found
        return res

    def non_matches( self ) :
        res = self.nr_matches
        res += self.n_reversed
        res += self.nr_reversed
        res += self.n_no_match
        res += self.nr_no_match
        res += self.n_not_found
        return res

def extract_matches( d1, d2 ) :
    res = 0
    l = []
    for bases in d1 :
        if bases in d2 :
            l.append( bases )
    for bases in l :
        l1 = d1[ bases ]
        l2 = d2[ bases ]
        if len( l1 ) == 1 and len( l2 ) == 1 :
            if l1[ 0 ] == l2[ 0 ] :
                res += 1
                del d1[ bases ]
                del d2[ bases ]
    return res


def join_2( q1, q2, q3 ) :
    #the key is the bases
    #the value is a ist of qualities
    d1 = {}
    d2 = {}
    
    n1 = 0
    n2 = 0
    r1 = 1
    r2 = 1
    loop = 0
    
    js = join_stats()
    
    while ( r1 + r2 ) > 0 :
        loop += 1
        if loop % 10000 == 0 :
            js.n_matches += extract_matches( d1, d2 )
            #sys.stdout.write( '.' )
            #sys.stdout.flush()
            
        if r1 > 0 :
            r1 = handle_q( d1, q1 )
            n1 += r1
        if r2 > 0:
            r2 = handle_q( d2, q2 )
            n2 += r2
            
    print( "from fastq-dump : %d" % n1 )
    print( "from sam-dump   : %d" % n2 )
    
    for bases in d1 :
        ql_1 = d1[ bases ]
        ql_2 = d2.get( bases, None )
        if ql_2 != None :
            ( m, r, n ) = compare_lists( ql_1, ql_2 )
            if n > 0 :
                ql_2 = d2.get( reverse_compl( bases ), None )
                if ql_2 != None :
                    ( m1, r1, n1 ) = compare_lists( ql_1, ql_2 )
                    js.add_rmatch_res( m1, r1, n1 )
            else :
                js.add_match_res( m, r, n )
        else :
            ql_2 = d2.get( reverse_compl( bases ), None )
            if ql_2 != None :
                ( m2, r2, n2 ) = compare_lists( ql_1, ql_2 )
                js.add_rmatch_res( m2, r2, n2 )
            else :
                js.n_not_found += 1
    
    print( js )
    q3.put( js.non_matches() )

    
if __name__ == '__main__':
    print( "running: ", __file__ )

    if sys.version_info[ 0 ] < 3 :
        print( "does not work with python version < 3!" )
        sys.exit( 3 )

    acc = 'SRR3332402'
    spots = None
    fastq_dump = 'fastq-dump'
    sam_dump = 'sam-dump'
    
    short_opts = "ha:s:f:m:"
    long_opts = [ "acc=", "spots=", "fastq_dump=", "sam_dump=" ]
    try :
        opts, args = getopt.getopt( sys.argv[ 1: ], short_opts, long_opts )
    except getopt.GetoptError :
        print ( sys.argv[ 0 ], ' -a <accession> -s <spots> -f <fastq-dump-binary> -m <sam-dump-binary>' )
        sys.exit( 2 )
    for opt, arg in opts :
        if opt == '-h' :
            print ( sys.argv[ 0 ], ' -a <accession> -s <spots> -f <fastq-dump-binary> -m <sam-dump-binary>' )
            sys.exit()
        elif opt in ( "-a", "--acc" ) :
            acc = arg
        elif opt in ( "-s", "--spots" ) :
            spots = int( arg )
        elif opt in ( "-f", "--fastq_dump" ) :
            fastq_dump = arg
        elif opt in ( "-m", "--sam_dump" ) :
            sam_dump = arg

    print( "accession = ", acc )
    if spots != None :
        print( "spots = ", spots )
   
    q1 = multiprocessing.Queue()
    q2 = multiprocessing.Queue()
    q3 = multiprocessing.Queue()
    
    p1 = multiprocessing.Process( target = fastq_dump_full, args = ( fastq_dump, acc, spots, q1 ), )
    p2 = multiprocessing.Process( target = sam_dump_full, args = ( sam_dump, acc, spots, q2 ), )
    p3 = multiprocessing.Process( target = join_2, args = ( q1, q2, q3 ), )
    
    p1.start()
    p2.start()
    p3.start()
    
    p1.join()
    p2.join()
    p3.join()
    
    nm = q3.get()
    print( "non-matches : ", nm )
    if nm > 0 :
        sys.exit( 3 )
    print( "success: ", __file__, "\n" )
