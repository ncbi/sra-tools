#!/usr/bin/env python

import sys, os, shutil, argparse, sqlite3, subprocess, hashlib, time
from enum import Enum

Task = Enum( 'Task', [ 'INIT', 'RUN', 'ADD', 'DEL', 'EXP', "LS" ] )

def extract_task( fn ) :
    if fn == None :
        return Task.RUN
    f = fn.upper()
    if f == 'RUN' :
        return Task.RUN
    if f == 'INIT' :
        return Task.INIT
    if f == 'ADD' :
        return Task.ADD
    if f == 'DEL' :
        return Task.DEL
    if f == 'EXP' :
        return Task.EXP
    if f == 'LS' :
        return Task.LS
    return None

def md5sum( filename ) :
    with open( filename, mode = 'rb' ) as f:
        d = hashlib.md5()
        while True:
            buf = f.read( 4096 )
            if not buf :
                break
            d.update( buf )
        return d.hexdigest()

def md5_of_dir( a_dir ) :
    a = []
    if os.path.isdir( a_dir ) :
        try :
            for root, dirs, files in os.walk( a_dir ) :
                for f in files:
                    md5 = md5sum( os.path.join( root, f ) )
                    a . append( f"{f}:{md5}" )
        except Exception as e:
            print( f"md5_of_dir() error : {e}" )
    a.sort()
    return '-'.join( a )

def clear_dir( a_dir, delete_self = False ) :
    if os.path.isdir( a_dir ) :
        try :
            for root, dirs, files in os.walk( a_dir ) :
                for f in files:
                    os.unlink( os.path.join(root, f ) )
                for d in dirs:
                    shutil.rmtree( os.path.join( root, d ) )
            if delete_self :
                shutil.rmtree( a_dir )
        except :
            pass

def min_sec( t1, t2 ) :
    t = round( t2 - t1, 1 )
    minutes = int( t / 60 )
    seconds = int( t - ( minutes * 60 ) )
    return f"{minutes} min {seconds} sec"

def run_external( cmd, use_shell = False ) :
    try :
        start_time = time.time()
        res = subprocess.run( cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell = use_shell )
        std_out = res.stdout.decode('utf-8')
        std_err = res.stderr.decode('utf-8')
        ret = res.returncode
        end_time = time.time()        
        return ( ret, std_out, std_err, min_sec( start_time, end_time ) )
    except Exception as e :
        print( f"run_external() error : {e}" )
        return ( -1, None, None, 0 )
    
def check_locator( locator_script, acc ) :
    if os.path.isfile( locator_script ) :
        ret = run_external( f"{locator_script} {acc}", True )
        if ret[ 0 ] == 0 and ret[ 1 ] != None :
            return ret[ 1 ].strip()
    return acc

def test_count( args ) :
    try :
        con = sqlite3.connect( args.db )
        res = con.cursor().execute( "SELECT count(*) FROM test" )
        cnt = res.fetchone()
        if cnt == None :
            return 0
        return int( cnt[0] )
    except Exception as e :
        print( f"test_count() db='{args.db}' error:{e}" )
        sys.exit( 3 )

def clear_test_path_and_enter( test_path ) :
    cur_path = os.getcwd()
    if not os.path.isdir( test_path ) :
        os . mkdir( test_path )
    else :
        clear_dir( test_path )
    os . chdir( test_path )
    return cur_path

def get_one_test( args, nr ) :
    try :
        con = sqlite3.connect( args.db )
        res = con.cursor().execute( f"SELECT * FROM test LIMIT 1 OFFSET {nr}" )
        return res.fetchone()
    except Exception as e :
        print( f"get_one_test db='{args.db}' eror : {e}" )
        sys.exit( 3 )

def get_test_by_name( args ) :
    try :
        con = sqlite3.connect( args.db )
        res = con.cursor().execute( f"SELECT * FROM test WHERE name = '{args.name}'" )
        return res.fetchone()
    except Exception as e :
        print( f"get_test_by_name db='{args.db}', name={args.name} error : {e}" )
        return None

def run_one_test( args, nr, count, abs_exe, test_data ) :
    if test_data == None :
        return false;
    if len( test_data[ 3 ] ) == 0 :
        print( f"test #{nr} of {count} ... skipped, because no expected values" )
        return True
    try :
        acc = check_locator( args.loc, test_data[ 1 ] )
        test_path = f"test_{nr}"
        cur_path = clear_test_path_and_enter( test_path )
        cmd = f"{abs_exe} {acc} {test_data[ 2 ]}"        
        print( f"test #{nr} of {count} ... running: {cmd}" )
        res = run_external( cmd, True )
        os . chdir( cur_path )
        if res[ 0 ] != 0 :
            print( f"test #{nr} of {count} ... tool FAILED ( {res[ 0 ]} )\n" )
            if not args.keep :
                clear_dir( test_path, True )          
            return False
        calculated = md5_of_dir( test_path )
        if not args.keep :
            clear_dir( test_path, True )
        if calculated != test_data[ 3 ] :
            print( f"test #{nr} of {count} ... expected values FAILED ( {res[ 3 ]} )\n" )
            return False
        print( f"test #{nr} of {count} ... SUCCESS ( {res[ 3 ]} )\n" )
        return True
    except Exception as e :
        print( f"run_one_test() error = {e}" )
        if not args.keep :
            clear_dir( test_path, True )
        return False

def check_dependencies( args ) :
    if not os.path.isfile( args.exe ) :
        print( f"executable to be tested '{args.exe}' is missing" )
        return 0
    count = test_count( args )
    if count < 1 : 
        print( f"no tests found in db'{args.db}'" )
    return count
    
def run_tests( args ) :
    count = check_dependencies( args )
    if count < 1 : 
        sys.exit( 3 )
    success = 0
    abs_exe = os . path . abspath( args.exe )
    t_start = time.time()
    if args.name != None :
        count = 1
        test_data = get_test_by_name( args )
        if run_one_test( args, 1, count, abs_exe, test_data ) :
            success = success + 1
    else :
        for nr in range( count ) :
            test_data = get_one_test( args, nr )
            if run_one_test( args, nr + 1, count, abs_exe, test_data ) :
                success = success + 1
    duration = min_sec( t_start, time.time() )
    if success < count :
        print( f"SUCCESS for {success} of the {count} tests in {duration}" )
        sys.exit( 3 )
    print( f"SUCCESS for all {count} tests in {duration}" )

def exp_one_test( args, nr, count, abs_exe, test_data ) :
    if test_data == None :
        return False
    if len( test_data[ 3 ] ) > 0 and not args.force :
        print( f"test # {nr} has already expected data" )
        return True
    try :
        acc = check_locator( args.loc, test_data[ 1 ] )
        test_path = f"test_{nr}"
        cur_path = clear_test_path_and_enter( test_path )
        opt=test_data[ 2 ]
        cmd = f"{abs_exe} {acc} {opt}" if len( opt ) > 0 else f"{abs_exe} {acc}"
        print( f"test # {nr} of {count} ... running: >{cmd}<" )
        res = run_external( cmd, True )
        os . chdir( cur_path )
        if res[ 0 ] != 0 :
            print( f"test# {nr} of {count} ... tool FAILED result={res[0]} ( {res[3]} )\n" )
            if not args.keep :
                clear_dir( test_path, True )
            return False
        a = md5_of_dir( test_path )        
        if not args.keep :
            clear_dir( test_path, True )
        con = sqlite3.connect( args.db )
        test_name = test_data[ 0 ]
        con.cursor().execute( f"UPDATE test SET expected = '{a}' WHERE name='{test_name}'" )
        con.commit()
        print( f"test# {nr} of {count} : expected values stored ( {res[ 3 ]} )\n" )
        return True
    except Exception as e :
        print( f"exp_one_test() error: {e}" )
        if not args.keep :
            clear_dir( test_path, True )
        return False

def exp_tests( args ) :
    count = check_dependencies( args )
    if count < 1 : 
        sys.exit( 3 )
    success = 0
    abs_exe = os . path . abspath( args.exe )
    if args.name != None :
        count = 1
        test_data = get_test_by_name( args )
        if exp_one_test( args, 1, count, abs_exe, test_data ) :
            success = success + 1
    else :
        for nr in range( count ) :
            test_data = get_one_test( args, nr )
            if exp_one_test( args, nr + 1, count, abs_exe, test_data ) :
                success = success + 1
    if success < count :
        print( f"SUCCESS for {success} of the {count} tests" )        
        sys.exit( 3 )
    print( f"SUCCESS for all {count} tests" )

def does_test_exist( args ) :
    try :
        con = sqlite3.connect( args.db )
        res = con.cursor().execute( f"SELECT name FROM test WHERE name='{args.name}'" )
        return res.fetchone() != None
    except Exception as e :
        print( f"does_test_exist( {args.db}, {args.name} ) error: {e}" )
        sys.exit( 3 )

def perform_add_test( args ) :
    opt = args.opt
    opt2 = []
    for o in opt :
        if len( o ) > 1 and o[ 0 ] == '\'' :
            o = o[ 1: ]
        if len( o ) > 1 and o[ -1 ] == '\'' :
            o = o[ :-1 ]
        opt2.append( o )
    opt3 = " ".join( opt2 ) if len( opt2 ) > 0 else ""
    try :
        print( f"adding test '{args.name}' for '{args.acc}' - options = '{opt3}'" )
        con = sqlite3.connect( args.db )
        cur = con.cursor()
        cur.execute( f"INSERT INTO test VALUES ( '{args.name}', '{args.acc}', '{opt3}', '' )" )
        con.commit()
    except Exception as e :
        print( f"perform_add_test( {args.db} ) error : {e}" )
        sys.exit( 3 )

def remove_test( args ) :
    try :
        con = sqlite3.connect( args.db )
        res = con.cursor().execute( f"DELETE FROM test WHERE name='{args.name}'" )
        con.commit()
    except Exception as e :
        print( f"remove_test( {args.db} ) error : {e}" )
        sys.exit( 3 )

def add_test( args ) :
    if args.name == None or args.name == '' :
        print( f"adding test: missing name" )
        sys.exit( 3 )
    if args.acc == '' :
        print( f"adding test '{args.name}' - accession is missing" )
        sys.exit( 3 )
    if does_test_exist( args ) :
        if args.force :
            remove_test( args )
        else :
            print( f"adding test '{args.name}' - name does already exist" )
            sys.exit( 3 )
    perform_add_test( args )

def del_test( args ) :
    if args.name == None or args.name == '' :
        print( f"removing test: missing name" )
        sys.exit( 3 )
    print( f"removing test '{args.name}'" )
    remove_test( args )

def print_test_data( args, nr, test_data ) :
    if test_data != None :
        data = "D" if test_data[ 3 ] != '' else "X"
        cmd = f"{args.exe} {test_data[ 1 ]} {test_data[ 2 ]}"
        print( f"test #{nr}: {test_data[0]} {data} = {cmd}" )
    
def list_tests( args ) :
    count = test_count( args )
    if args.name != None :
        print( f"listing test named {args.name}:" )
        print_test_data( args, 0, get_test_by_name( args ) )
    else :
        print( f"listing {count} tests:" )
        for nr in range( count ) :
            print_test_data( args, nr, get_one_test( args, nr ) )

def initialize_db( args ) :
    print( f"initializing db: '{args.db}'" )
    try :
        con = sqlite3.connect( args.db )
        cur = con.cursor()
        cur.execute( "CREATE TABLE IF NOT EXISTS test( name, acc, options, expected )" )
        con.commit()
    except Exception as e :
        print( f"initialize_db( {args.db} ) error : {e}" )        

def cmdline_parsing() :
    parser = argparse.ArgumentParser( description='testing fasterq-dump' )
    parser.add_argument( '--init', action='store_true', help='initialize db (dflt:False)' )
    parser.set_defaults( init = False )
    parser.add_argument( '--force', action='store_true', help='force initialization (dflt:False)' )
    parser.set_defaults( force = False )
    parser.add_argument( '--keep', action='store_true', help='keep files produced by tests (dflt:False)' )
    parser.set_defaults( keep = False )
    parser.add_argument( '--db', nargs='?',  default='tests.db', help='database to be used (dflt:tests.db)' )
    parser.add_argument( '--exe', nargs='?', default='./fasterq-dump', help='executable (dflt:./fasterq-dump)' )
    parser.add_argument( '--fn', nargs='?', default=None, help='selecting function: init|run|add|del|exp|ls (dflt:run)' )
    parser.add_argument( '--name', nargs='?', default=None, help='name of test to run,add,del,exp' )
    parser.add_argument( '--opt', nargs='*', default='', help='options for adding a test' )
    parser.add_argument( '--acc', nargs='?', default=None, help='accession for adding a test' )
    parser.add_argument( '--loc', nargs='?', default='./locator.sh', help='optional accession locator-script' )    
    return parser.parse_args()

if __name__ == '__main__' :
    args = cmdline_parsing()
    task = extract_task( args.fn )
    if not os.path.isfile( args.db ) :
        if task == Task.INIT :
            initialize_db( args )
            sys.exit( 0 )
        else :
            print( f"database '{args.db}' is missing" )
            sys.exit( 3 )
    if task == Task.INIT :
        if args.force :
            initialize_db( args )
        else :
            print( f"database '{args.db}' already exists, skipping initialization" )
    elif task == Task.ADD :
        add_test( args )
    elif task == Task.DEL :
        del_test( args )
    elif task == Task.EXP :
        exp_tests( args )
    elif task == Task.LS :
        list_tests( args )
    elif task == Task.RUN :
        run_tests( args )
    else :
        print( f"unknown task '{args.fn}'" )
        sys.exit( 3 )
