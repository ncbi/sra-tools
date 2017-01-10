#!/usr/bin/env python

import sys, getopt, subprocess, multiprocessing


def run( cmd, lines, q_out ) :
    p = subprocess.Popen( cmd, stdout = subprocess.PIPE )
    n = 0
    bases = None
    while True :
        line = p.stdout.readline()
        if line != '' :
            n += 1
            if not line.startswith( '@' ) :
                q_out.put( line )
            if lines != None :
                if n > lines :
                    p.kill()
                    break
        else :
            break
    q_out.put( None )


def sam_dump_full( sam_dump, acc, spots, q_out ) :
    run( [ sam_dump, '-u', '-g', acc ], spots, q_out )
    print "sam-dump done"


def count_lines( q_in, q_out ) :
    total = 0
    with_spotgrp = 0
    while True :
        line = q_in.get()
        if line != None :
            total += 1
            sam = line.split( '\t' )
            qname = sam[ 0 ]
            name_parts = qname.split( '.' )
            if len( name_parts ) > 1 :
                with_spotgrp += 1
        else :
            break
    res = ( total, with_spotgrp )
    q_out.put( res )


if __name__ == '__main__':
    acc = 'SRR3332402'
    spots = None
    sam_dump = 'sam-dump'
    
    short_opts = "ha:s:m:"
    long_opts = [ "acc=", "spots=", "sam_dump=" ]
    try :
        opts, args = getopt.getopt( sys.argv[ 1: ], short_opts, long_opts )
    except getopt.GetoptError :
        print sys.argv[ 0 ], ' -a <accession> -s <spots> -m <sam-dump-binary>'
        sys.exit( 2 )
    for opt, arg in opts :
        if opt == '-h' :
            print sys.argv[ 0 ], ' -a <accession> -s <spots> -m <sam-dump-binary>'
            sys.exit()
        elif opt in ( "-a", "--acc" ) :
            acc = arg
        elif opt in ( "-s", "--spots" ) :
            spots = int( arg )
        elif opt in ( "-m", "--sam_dump" ) :
            sam_dump = arg

    print 'accession = ', acc
    if spots != None :
        print 'spots = ', spots
   
    q1 = multiprocessing.Queue()
    q2 = multiprocessing.Queue()
    
    p1 = multiprocessing.Process( target = sam_dump_full, args = ( sam_dump, acc, spots, q1 ), )
    p2 = multiprocessing.Process( target = count_lines, args = ( q1, q2 ), )
    
    p1.start()
    p2.start()
    
    p1.join()
    p2.join()
    
    res = q2.get()
    print "total        : ", res[ 0 ]
    print "with_spotgrp : ", res[ 1 ]
    if res[ 0 ] != res[ 1 ] :
        print "not all sam-lines have a spotgroup in the QNAME-field!"
        sys.exit( 3 )
