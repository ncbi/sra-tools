
import sys, os, random, re

def make_py_vdb_dir( path ) :
    base, leaf = os.path.split( path )
    return os.path.join( base, "py_vdb" )

def make_vdb_wr_lib( path ) :
    res = os.path.join( vdb_lib_dir, r'libncbi-wvdb.so' )
    if os.path.exists( res ) :
        return res
    return os.path.join( vdb_lib_dir, r'libncbi-wvdb.dylib' )

def randomRead( length : int ):
    y = []
    while len( y ) <= length:
        y += [ 'A', 'C', 'G', 'T' ]
    # y is ACGT repeated enough times that there are at least length elements
    return ''.join( random . sample( y, k = length ) )

def phred33( x : int ) :
    return chr( x + 33 )

def randomQuality( length : int ) :
    y = []
    # some values will <20, but never enough to have a chance to trigger any
    # of the other rules, regardless of how they are arranged
    while len ( y ) * 4 <= length and len( y ) < 9:
        y . append( random . randrange( 2, 20 ) )
    while len ( y ) < length:
        y . append( random . randrange( 20, 41 ) )
    random . shuffle( y )
    return ''.join( map( phred33, y ) )[ 0 : length ]

cursDef = {
     'READ_FILTER': {
         'expression': '(U8)READ_FILTER',
     },
     'READ_TYPE': {
         'expression': '(U8)READ_TYPE',
         'default': [1, 1],
     },
     'NAME': {
         'expression': '(ascii)NAME',
     },
     'READ_START': {
         'expression': '(INSDC:coord:zero)READ_START',
     },
     'READ_LEN': {
         'expression': '(INSDC:coord:len)READ_LEN',
     },
     'READ': {
         'expression': '(INSDC:dna:text)READ',
     },
     'QUALITY': {
         'expression': '(INSDC:quality:text:phred_33)QUALITY',
     },
}

def write_row( curs, cols, good, length, name ) :
    curs.OpenRow()
    cols[ 'NAME' ].write( name )
    if good :
        cols[ 'READ_FILTER' ].write( [ 0, 0 ] )
    else:
        cols[ 'READ_FILTER' ].write( [ 3, 3 ] )
    cols[ 'READ_START' ].write ( [ 0, length ] )
    cols[ 'READ_LEN' ].write( [ length, length]  )
    cols[ 'READ' ].write( randomRead( length * 2 ) )
    cols[ 'QUALITY' ].write( randomQuality( length ) + randomQuality( length ) )
    curs.CommitRow()
    curs.CloseRow()

def generate_output( output, vdb_inc_dirs, vdb_wr_lib ) :
    mgr = manager( OpenMode.Write, vdb_wr_lib )
    print( f"info: VDBManager: {mgr.Version()} from {vdb_wr_lib}" )
    schema = mgr.MakeSchema()
    includes = vdb_inc_dirs.split(":")
    for incl in includes:
        schema.AddIncludePath( incl )
    schema.ParseFile( 'sra/generic-fastq.vschema' )
    db = mgr.CreateDB( schema, 'NCBI:SRA:GenericFastq:db', output )
    tbl = db.CreateTable( 'SEQUENCE' )
    curs = tbl.CreateCursor(OpenMode.Write)
    cols = {}
    for name, define in cursDef.items():
        cols[ name ] = curs.AddColumn( define[ 'expression' ] )
    curs.Open()
    for col in cols.values():
        col._update()
    for name, define in cursDef.items():
        if 'default' in define:
            cols[ name ].set_default( define[ 'default' ] )
    write_row( curs, cols, True, 75, 'first' )
    write_row( curs, cols, True, 50, 'second' )
    write_row( curs, cols, False, 75, 'third' )
    write_row( curs, cols, False, 50, 'forth' )
    curs.Commit()

if __name__ == '__main__':
    #check if we are running on linux/mac
    platform = sys.platform
    if platform != 'linux' and platform != 'darwin' and platform != 'freebsd14':
        print( f"wrong platform: {platform}" )
        sys.exit( 0 )

    #we need 4 parameters:
    if len( sys.argv ) != 5 :
        print( "we need 4 parameters: output, schema-include-paths, vdb-library-dir, vdb.py dir" )
        sys.exit( 1 )

    #output defaults to 'test-data'

    output      = sys.argv[ 1 ]
    vdb_inc_dirs = sys.argv[ 2 ]
    vdb_lib_dir = sys.argv[ 3 ]
    py_vdb_dir  = sys.argv[ 4 ]
    vdb_wr_lib  = make_vdb_wr_lib( vdb_lib_dir )

    # can be a ':'-separated list of directories
    # if not os.path.exists( vdb_inc_dirs ) :
    #     print( f"{vdb_inc_dirs} does not exist!" )
    #     sys.exit( 1 )

    if not os.path.exists( vdb_lib_dir ) :
        print( f"{vdb_lib_dir} does not exist!" )
        sys.exit( 1 )

    if not os.path.exists( vdb_wr_lib ) :
        print( f"{vdb_wr_lib} does not exist!" )
        sys.exit( 1 )

    if not os.path.exists( py_vdb_dir ) :
        print( f"{py_vdb_dir} does not exist!" )
        sys.exit( 1 )

    print( f"output      = {output}" )
    print( f"vdb_inc_dirs = {vdb_inc_dirs}" )
    print( f"py_vdb_dir  = {py_vdb_dir}" )
    print( f"vdb_wr_lib  = {vdb_wr_lib}" )

    saveSysPath = sys.path
    sys.path.append( py_vdb_dir )
    from vdb import *
    sys.path = saveSysPath

    generate_output( output, vdb_inc_dirs, vdb_lib_dir ) #vdb_wr_lib )
