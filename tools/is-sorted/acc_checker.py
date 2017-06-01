#! /usr/bin/env python

import sys
import os
import subprocess
import sqlite3

def check_output_no_stderr( *popenargs, **kwargs ) :
    if 'stdout' in kwargs :
        raise ValueError( 'stdout argument not allowed, it will be overridden.' )
    if 'stderr' in kwargs :
        raise ValueError( 'stderr argument not allowed, it will be overridden.' )
    process = subprocess.Popen( stdout=subprocess.PIPE, stderr=subprocess.PIPE, *popenargs, **kwargs )
    output, unused_err = process.communicate()
    retcode = process.poll()
    #if retcode :
    #    cmd = kwargs.get( "args" )
    #    if cmd is None:
    #        cmd = popenargs[ 0 ]
    #    raise subprocess.CalledProcessError( retcode, cmd )
    return output, retcode

def get_accessions_and_lock( sra_db, count, rowlimit ) :
    ret = list()
    try :
        with sqlite3.connect( sra_db ) as conn :
            cur = conn.cursor()
            if rowlimit < 1 :
                cur.execute( "SELECT ACC FROM CSRA WHERE STATE = 0 LIMIT %d"%count )
            else :
                cur.execute( "SELECT ACC FROM CSRA WHERE STATE = 0 AND ROWS < %d ORDER BY RANDOM() LIMIT %d"%( rowlimit, count ) )
            while True :
                row = cur.fetchone()
                if row == None :
                    break
                ret.append( row[ 0 ] )
            for acc in ret :
                cur.execute( "UPDATE CSRA SET STATE = 1 WHERE ACC = '%s'"%acc )
    except sqlite3.OperationalError as err :
        print( err )
    return ret

def set_state_and_sorted( sra_db, acc, sorted ) :
    try :
        with sqlite3.connect( sra_db ) as conn :
            cur = conn.cursor()
            cur.execute( "UPDATE CSRA SET STATE = 2 WHERE ACC = '%s'"%( acc ) )
            cur.execute( "UPDATE CSRA SET SORTED = %d WHERE ACC = '%s'"%( sorted, acc ) )
    except sqlite3.OperationalError as err :
        print( err )


def perform_parse( sra_db, count, rowlimit ) :
    accessions = get_accessions_and_lock( sra_db, count, rowlimit )
    for acc in accessions :
        print( "checking : %s"%acc )
        output, retcode = check_output_no_stderr( [ 'is-sorted', acc ] )
        print( "output: %s"%output )
        print( "ret   : %d"%retcode )
        if retcode == 0 :
            set_state_and_sorted( sra_db, acc, 1 )
        else :
            set_state_and_sorted( sra_db, acc, 2 )
                
if __name__ == '__main__':
    sra_db = "to_inspect.db"
    count = 5
    rowlimit = 200000
    if len( sys.argv ) > 1 :
        sra_db = sys.argv[ 1 ]

    if len( sys.argv ) > 2 :
        count = int( sys.argv[ 2 ] )

    if len( sys.argv ) > 3 :
        rowlimit = int( sys.argv[ 3 ] )

    if os.path.isfile( sra_db ) :
        perform_parse( sra_db, count, rowlimit )
    else :
        print( "'%s' does not exist!" % sra_db )
