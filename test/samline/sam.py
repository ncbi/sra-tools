import subprocess, os, shutil, sys, random

def get_tool( env_name, tool_name ) :
    try :
        env_dir = os.environ[ env_name ]
        if len( env_dir ) == 0 :
            env_ir = "."
        return os.path.join( env_dir, tool_name )
    except :
        return None

def try_cmd( cmd, err ) :
    try :
        res = subprocess.check_output( cmd, shell = True )
        return res.decode( 'utf-8' )
    except Exception as e:
        print( f"{err} -> {e}" )
    return None

def to_int( s, dflt = 0 ) :
    if None != s :
        try :
            return int( s )
        except :
            try : 
                return int( s, 16 )
            except :
                pass
    return dflt

def shuffle( lst ) :
    for i in range( len( lst) - 1, 0, -1 ) :
        # Pick a random index from 0 to i
        j = random.randint( 0, i + 1 )
        # Swap arr[i] with the element at random index
        lst[ i ], lst[ j ] = lst[ j ], lst[ i ]

'''---------------------------------------------------------------
helper function: generate READ from cigar,refname,refpos
---------------------------------------------------------------'''
def len_of_ref( ref ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    if None != tool :
        cmd = f"{tool} -f rlen -r {ref}"
        res = try_cmd( cmd, "sam.py:len_of_ref()" )
        return to_int( res )
    return None

'''---------------------------------------------------------------
helper function: generate READ from cigar,refname,refpos
---------------------------------------------------------------'''
def cigar2read( cigar, pos, ref ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    if None != tool :
        cmd = f"{tool} -f read -c {cigar} -p {pos} -r {ref}"
        return try_cmd( cmd, "sam.py:cigar2read()" )
    return None

'''---------------------------------------------------------------
helper function: generate random QUALITY of given length
---------------------------------------------------------------'''
def rnd_qual( l ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    if None != tool :
        cmd = f"{tool} -f qual -l {l} -s 7"
        return try_cmd( cmd, "sam.py:rnd_qual()" )
    return None

'''---------------------------------------------------------------
helper function: transform cigar with given inserts into
a 'clean' cigar, bam-load does accept
---------------------------------------------------------------'''
def merge_cigar( cigar ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    if None != tool :
        cmd = f"{tool} -f cigar -c {cigar}"
        return try_cmd( cmd, "sam.py:merge_cigar()" )
    return None

'''---------------------------------------------------------------
helper function: to get length of a reference ( from the RefSeq-Acc )
---------------------------------------------------------------'''
def ref_len( ref ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    if None != tool :
        cmd = f"{tool} -f rlen -r {ref}"
        return try_cmd( cmd, "sam.py:ref_len()" )
    return None

'''---------------------------------------------------------------
helper function: remove a file, without error if it does not exist
---------------------------------------------------------------'''
def rm_file( filename ) :
    try:
        os.remove( filename )
    except:
        pass

'''---------------------------------------------------------------
helper function: remove a direcotry, without error if it does not exist
---------------------------------------------------------------'''
def rm_dir( dirname ) :
    try:
        shutil.rmtree( dirname, ignore_errors=True )
    except:
        pass

def load_file( filename ) :
    if os.path.isfile( filename ) :
        with open( filename, "r" ) as the_file:
            return the_file.read()
    return ""

def print_file( filename ) :
    s = load_file( filename )
    if len( s ) > 0 :
        print( s )

def print_txt( txt ) :
    if txt != None :
        if len( txt ) > 0 :
            print( txt )

def print_txt_list( lst ) :
    for a in lst :
        if a != None :
            print_txt( a )

'''===============================================================
preform a bam-load on a python-list of SAM-objects
    will create temporary files and directory ( x.sam, x.kfg, x_csra )
    writes the content of the python-list into x.sam
    writes a config file into x.kfg
    performs bam-load, and prints it's output
    kar's the created directory into the given output-file
    can be asked to keep the temporary files
    list ........ list of SAM-objects
    output....... name of cSRA-file to be created
    params....... parameters passed into bam-load
    keep_files... False/True for debugging temp. files
==============================================================='''
def bam_load( list, output, params, keep_files = False ) :
    rm_dir( "x_csra" )
    rm_file( output )
    rm_file( "err.txt" )
    save_sam( list, "x.sam" )
    save_config( list, "x.kfg" )

    tool = get_tool( "BINDIR", "bam-load" )
    if None == tool :
        return 0
    
    cmd = f"{tool} {params} -o x_csra -k x.kfg x.sam 2>err.txt"
    txt1 = try_cmd( cmd, "sam.py:bam_load( bam-load )" )

    tool = get_tool( "BINDIR", "kar" )
    if None == tool :
        return 0

    cmd = f"{tool} --create {output} -d x_csra -f 2>err.txt"
    txt2 = try_cmd( cmd, "sam.py:bam_load( kar )" )

    if not keep_files :
        rm_dir( "x_csra" )
        rm_file( "x.sam" )
        rm_file( "x.kfg" )

    print_txt_list( [ load_file( "err.txt" ), txt1, txt2 ] )
    rm_file( "err.txt" )
    return 1


'''===============================================================
perform a sra-sort on a given cSRA-file
    will create a temporary directory ( x_csra )
    performs sra-sort, and prints it's output
    kar's the created directory into the given output-file
    can be asked to keep the temporary files
    src ......... source vdb-object ( cSRA )
    output....... name of cSRA-file to be created
    params....... parameters passed into bam-load
    keep_files... False/True for debugging temp. files
==============================================================='''
def sra_sort( src, output, params = "", keep_files = False ) :
    rm_dir( "s_csra" )
    rm_file( output )
    rm_file( "err.txt" )

    tool = get_tool( "BINDIR", "sra-sort" )
    if None == tool :
        return 0

    cmd = f"{tool} {src} s_csra -f {params} 2>err.txt"
    txt1 = try_cmd( cmd, "sam.py:sra_sort( sra-sort - phase )" )

    tool = get_tool( "BINDIR", "kar" )
    if None == tool :
        return 0

    cmd = f"{tool} --create {output} -d s_csra -f 2>err.txt"
    txt2 = try_cmd( cmd, "sam.py:sra_sort( kar - phase )" )
    if not keep_files :
        rm_dir( "s_csra" )
    print_txt_list( [ load_file( "err.txt" ), txt1, txt2 ] )
    rm_file( "err.txt" )
    return 1

'''===============================================================
perform sra-stat on a given cSRA-file
    will create an output-file in XML-form
    src ......... source vdb-object ( cSRA )
    output ...... file to produce
    params....... parameters passed into bam-load
==============================================================='''
def sra_stat( src, output ) :
    rm_file( output )
    
    tool = get_tool( "BINDIR", "sra-stat" )
    if None == tool :
        return 0

    cmd = f"{tool} -sx {src} >{output}"
    txt = try_cmd( cmd, "sam.py:sra_stat" )
    print_txt_list( [ txt ] )
    return 1

def vdb_dump( accession, params = "" ) :
    try :
        tool = get_tool( "BINDIR", "vdb-dump" )
        if None == tool :
            return 0

        cmd = f"{tool} {accession} {params}"
        txt = subprocess.check_output( cmd, stderr=subprocess.STDOUT, shell=True )
        s = txt.decode( "utf-8" )
        print( f"{s}" )
        return 1
    except Exception as e:
        print( f"sam.py:vdb_dump() -> {e}" )
    return 0

def sam_dump( accession, params = "" ) :
    try :
        tool = get_tool( "BINDIR", "sam-dump" )
        if None == tool :
            return 0
        cmd = f"sam-dump {accession} {params}"
        txt = subprocess.check_output( cmd, stderr=subprocess.STDOUT, shell=True )
        s = txt.encode( "utf-8" )
        print( f"{s}" )
        return 1
    except Exception as e:
        print( f"sam.py:sam_dump() -> {e}" )
    return 0

'''===============================================================
all 11 different SAM-Flags
==============================================================='''
FLAG_MULTI = 0x01
FLAG_PROPPER = 0x02
FLAG_UNMAPPED = 0x04
FLAG_NEXT_UNMAPPED = 0x08
FLAG_REVERSED = 0x010
FLAG_NEXT_REVERSED = 0x020
FLAG_FIRST = 0x040
FLAG_LAST = 0x080
FLAG_SECONDARY = 0x0100
FLAG_BAD = 0x0200
FLAG_PCR = 0x0400


'''===============================================================
    make a primary SAM-alignment
==============================================================='''
def make_prim( qname, flags, refname, refalias, pos, mapq, cigar, other = None ) :
    res = SAM( qname, flags | FLAG_PROPPER, refname, refalias, pos, mapq,
              merge_cigar( cigar ), cigar2read( cigar, pos, refname ), "*", 0 )
    if other != None :
        res.pair_with( other )
    return res


'''===============================================================
    make a secondary SAM-alignment
==============================================================='''
def make_sec( qname, flags, refname, refalias, pos, mapq, cigar, other = None ) :
    res = SAM( qname, flags | FLAG_SECONDARY, refname, refalias, pos, mapq,
              merge_cigar( cigar ), cigar2read( cigar, pos, refname ), "*", 0 )
    if other != None :
        res.link_to( other )
    return res

'''===============================================================
    make a unaligned SAM
==============================================================='''
def make_unaligned( qname, flags, seq ) :
    return SAM( qname, flags | FLAG_UNMAPPED, "-", "-", 0, 255, "*", seq, "-", 0 )


'''---------------------------------------------------------------
helper function: walk the list of SAM-objects, create a dictionary
    key: refalias, value: refname
    ( used in extract_headers and produce_config )
---------------------------------------------------------------'''
def make_refdict( list ) :
    res = {}
    for a in list :
        res[ a.refalias ] = a.refname
    return res

'''---------------------------------------------------------------
helper function: create SAM-headers from a list of SAM-objects
    as a list of strings
    used in print_sam and save_sam
---------------------------------------------------------------'''
def extract_headers( list ) :
    reflist = make_refdict( list )
    res = [ "@HD\tVN:1.3" ]
    for k, v in reflist.items():
        l = ref_len( v )
        res.append( f"@SQ\tSN:{k}\tAS:{k}\tLN:{l}" )
    return res

'''---------------------------------------------------------------
helper function: create a config-file for bam-load out of
    a list of SAM-objects
    used in save_config
---------------------------------------------------------------'''
def produce_config( list ) :
    reflist = make_refdict( list )
    res = []
    for k, v in reflist.items():
        if k != "*" and k != "-" :
            res.append( f"{k}\t{v}" )
    return res

'''---------------------------------------------------------------
helper function: save config file created from list of SAM-objects
    used in bam_load
---------------------------------------------------------------'''
def save_config( list, filename ) :
    with open( filename, "w" ) as f:
        for s in produce_config( list ) :
            f.write( f"{s}\n" )

'''---------------------------------------------------------------
helper function: prints a list of SAM-objects
---------------------------------------------------------------'''
def print_sam( list ):
    for s in extract_headers( list ) :
        print( s )
    for s in list :
        print( s )

'''---------------------------------------------------------------
helper function: save a list of SAM-objects as file
    used in bam_load
---------------------------------------------------------------'''
def save_sam( lst, filename ) :
    with open( filename, "w" ) as f:
        for s in extract_headers( lst ) :
            f.write( f"{s}\n" )
        for s in lst :
            f.write( f"{s}\n" )

'''===============================================================
    SAM-object
==============================================================='''
class SAM:

    def __init__( self, qname, flags, refname, refalias, pos, mapq, cigar, seq, rnxt, pnxt, tags="" ) :
        self.qname = qname
        self.flags = flags
        self.refname = refname        
        self.refalias = refalias
        self.pos = pos
        self.mapq = mapq
        self.cigar = cigar
        self.seq = seq
        if None == self.seq :
            self.seq = ""
        if None != self.seq :
            self.qual = rnd_qual( len( self.seq ) )
        else :
            self.qual = ""
        self.nxt_ref = rnxt
        self.nxt_pos = pnxt
        self.tlen = 0
        self.tags = tags

    def __str__( self ) :
        res = ""
        try :
            res = f"{self.qname}\t{self.flags}\t{self.refalias}\t{self.pos}\t{self.mapq}\t"
            res = res + f"{self.cigar}\t{self.nxt_ref}\t{self.nxt_pos}\t{self.tlen}\t"
            res = res + f"{self.seq}\t{self.qual}"
            if len( self.tags ) > 0 :
                res = res + f"\t{self.tags}"
        except Exception as e:
            print( f"sam.py:SAM.__str__() -> {e}" )
        return res

    def set_qual( self, min_qual, max_qual ) :
        l = len( self.seq )
        if l > 0 :
            if min_qual == max_qual :
                #generate a quality string of same length as self.seq
                c = 0x33 + min_qual
                self.qual = c * l
            else :
                pass
                
    def set_flag( self, flagbit, state ) :
        if state :
            self.flags |= flagbit
        else :
            self.flags &= ~flagbit

    def set_tags( self, tag ) :
        self.tags = tag

    def add_tag( self, tag ) :
        if len( self.tags ) > 0 :
            self.tags += ";"
            self.tags += tag
        else :
            self.tags = tag

    def is_prim( self ) :
        return self.flags & FLAG_PROPPER == FLAG_PROPPER

    def is_sec( self ) :
        return self.flags & FLAG_SECONDARY == FLAG_SECONDARY
        
    def pair_with( self, other ) :
        self.nxt_ref = other.refalias
        self.nxt_pos = other.pos
        self.qname = other.qname
        self.flags |= FLAG_MULTI
        other.nxt_ref = self.refalias
        other.nxt_pos = self.pos
        other.flags |= FLAG_MULTI
        self.set_flag( FLAG_FIRST, True )
        other.set_flag( FLAG_FIRST, False )
        self.set_flag( FLAG_LAST, False )
        other.set_flag( FLAG_LAST, True )
        self.set_flag( FLAG_NEXT_UNMAPPED, other.flags & FLAG_UNMAPPED )
        self.set_flag( FLAG_NEXT_REVERSED, other.flags & FLAG_REVERSED )
        other.set_flag( FLAG_NEXT_UNMAPPED, self.flags & FLAG_UNMAPPED )
        other.set_flag( FLAG_NEXT_REVERSED, self.flags & FLAG_REVERSED )

    def link_to( self, other ) :
        self.flags |= FLAG_MULTI

def get_dict_value( d, key, dflt ) :
    try :
        return d[ key ]
    except :
        pass
    return dflt
    
def get_name( d, dflt = "A" ) :
    return get_dict_value( d, 'name', dflt )

def get_flags( d, dflt = 0 ) :
    return to_int( get_dict_value( d, 'flags', "" ), dflt )

def get_seq( d, dflt = "ACGTACGTACGT" ) :
    return get_dict_value( d, 'seq', dflt )

def get_qual( d, dflt = "" ) :
    return get_dict_value( d, 'qual', dflt )

def get_noqual( d, dflt = "" ) :
    return get_dict_value( d, 'noqual', dflt )

def get_id0( d, dflt = 0 ) :
    return to_int( get_dict_value( d, 'id0', "" ), dflt )

def get_id1( d, dflt = 1 ) :
    return to_int( get_dict_value( d, 'id1', "" ), dflt )

def get_ref( d, dflt = "1" ) :
    return get_dict_value( d, 'ref', dflt )

def get_pos( d, dflt = 1 ) :
    return to_int( get_dict_value( d, 'pos', "" ), dflt )

def get_mapq( d, dflt = 20 ) :
    return to_int( get_dict_value( d, 'mapq', "" ), dflt )

def get_count( d, dflt = 50 ) :
    return to_int( get_dict_value( d, 'count', "" ), dflt )

def get_cigar( d, dflt = "50M" ) :
    return get_dict_value( d, 'cigar', dflt )

class SamBuilder :
    def __init__( self, filename = None ) :
        self.filename = filename
        self.refs = {}
        self.alignments = {}
        self.unaligned = list()

    def __str__( self ) :
        res = f"refs= { self.refs }\n"
        res += self.alignments_to_str()
        for a in self.unaligned :
            res += f"{ a }\n"
        return res

    def alignments_to_str( self ) :
        res = ""
        for k, v in self.alignments.items() :
            for a in v :
                res += f"{a}\n"
        return res

    def read( self ) :
        if self.filename == None :
            self.read_from_stdin ()
        else :
            self.read_from_file( self.filename )

    def read_from_stdin( self ) :
        for line in sys.stdin :
            self.handle_line( line.strip() )

    def read_from_file( self, filename ) :
        with open( filename, "r" ) as the_file:
            for line in the_file :
                self.handle_line( line.strip() )
            
    def handle_line( self, line ) :
        if line != None :
            if len( line ) > 2 :
                self.handle_line_validated( line )

    def handle_line_validated( self, line ) :
        d = {}
        for e in line[ 2: ].split( ',' ) :
            ae = e.strip().split( '=' )
            if len( ae ) == 2 :
                d[ ae[ 0 ] ] = ae[ 1 ]
        if line[ 0 ] == 'R' :
            self.refs[ d[ 'name' ] ] = d[ 'value' ]            
        elif line[ 0 ] == 'P' :
            self.add_prim( d )
        elif line[ 0 ] == 'A' :
            self.add_multiple_prim( d )
        elif line[ 0 ] == 'B' :
            self.add_multiple_sec( d )
        elif line[ 0 ] == 'C' :
            self.link_multiple( d )
        elif line[ 0 ] == 'S' :
            self.add_sec( d )
        elif line[ 0 ] == 'U' :
            #create unaligned item: name,flags,seq
            A = make_unaligned( get_name( d ), get_flags( d ), get_seq( d ) )
            self.unaligned.append( A )
        elif line[ 0 ] == 'L' :
            #pair 2 primary alignments : name,idx0,idx2
            self.link_alignments( get_name( d ), get_id0( d ), get_id1( d ) )

    def lookup_ref_alias( self, alias, dflt = "NC_011752.1" ) :
        try :
            return self.refs[ refalias ]
        except :
            pass
        return dflt

    def add_prim( self, d ) :
        #create primary alignment
        refalias = get_ref( d )
        ref = self.lookup_ref_alias( refalias )
        A = make_prim( get_name( d ), get_flags( d ), ref, refalias, get_pos( d ), get_mapq( d ), get_cigar( d ) )
        qual = get_qual( d )
        l = len( qual )
        if l == 1 :
            A.qual = qual * len( A.seq )
        noqual = get_noqual( d )
        if noqual == 'y' :
            A.qual = "*"
        self.add_alignment( A )

    def add_multiple_prim( self, d ) :
        #create multiple primary alignments
        #at a random-position on the reference, with a random-length
        namebase = get_name( d )
        flags = get_flags( d )
        refalias = get_ref( d )
        ref = self.lookup_ref_alias( refalias )
        ref_len = len_of_ref( ref )
        mapq = get_mapq( d )
        for i in range( 1, get_count( d ) + 1 ) :
            a_len = random.randint( 10, 100 )
            pos = random.randint( 1, ref_len - a_len )
            A = make_prim( f"{namebase}{i}", flags, ref, refalias, pos, mapq, f"{a_len}M" )
            self.add_alignment( A )

    def add_sec( self, d ) :
        refalias = get_ref( d )
        ref = self.lookup_ref_alias( refalias )
        A = make_sec( get_name( d ), get_flags( d ), ref, refalias, get_pos( d ), get_mapq( d ), get_cigar( d ) )
        self.add_alignment( A )

    def add_multiple_sec( self, d ) :
        #create multiple primary alignments: namebase,flags,refalias,mapq,count
        #at a random-position on the reference, with a random-length
        namebase = get_name( d )
        flags = get_flags( d )
        refalias = get_ref( d )
        ref = self.lookup_ref_alias( refalias )
        ref_len = len_of_ref( ref )
        mapq = get_mapq( d )
        for i in range( 1, get_count( d ) + 1 ) :
            a_len = random.randint( 10, 100 )
            pos = random.randint( 1, ref_len - a_len )
            A = make_sec( f"{namebase}{i}", flags, ref, refalias, pos, mapq, f"{a_len}M" )
            self.add_alignment( A )

    def add_alignment( self, a ) :
        if None != a :
            item = self.alignments.get( a.qname )
            if item == None :
                item = list()
                item.append( a )
                self.alignments[ a.qname ] = item
            else :
                item.append( a )

    def link_alignments( self, name, idx1, idx2 ) :
        item = self.alignments.get( name )
        if None != item :
            A0 = item[ idx1 ]
            A1 = item[ idx2 ]
            if None != A0 and None != A1 :
                if A0.is_prim() and A1.is_prim() :
                    A0.pair_with( A1 )
                elif A0.is_prim() and A1.is_sec() :
                    A1.link_to( A0 )
                elif A1.is_prim() and A0.is_sec() :
                    A0.link_to( A1 )

    def link_multiple( self, d ) :
        namebase = get_name( d )
        id0 = get_id0( d )
        id1 = get_id1( d )
        for i in range( 1, get_count( d ) + 1 ) :
            name = f"{namebase}{i}"
            self.link_alignments( name, id0, id1 )

    def to_list( self ) :
        res = list()
        for k, v in self.alignments.items() :
            for a in v :
                res.append( a )
        for a in self.unaligned :
            res.append( a )
        return res
