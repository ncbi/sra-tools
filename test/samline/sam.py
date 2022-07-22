import subprocess
import os
import shutil

def get_tool( env_name, tool_name ) :
    env_dir = os.environ[ env_name ]
    if len( env_dir ) == 0 :
        env_ir = "."
    return os.path.join( env_dir, tool_name )

def try_cmd( cmd, err ) :
    try :
        res = subprocess.check_output( cmd, shell = True )
        return res.decode( 'utf-8' )
    except Exception as e:
        print( f"{err} -> {e}" )
    return None

'''---------------------------------------------------------------
helper function: generate READ from cigar,refname,refpos
---------------------------------------------------------------'''
def cigar2read( cigar, pos, ref ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    cmd = f"{tool} -f read -c {cigar} -p {pos} -r {ref}"
    return try_cmd( cmd, "sam.py:cigar2read()" )
    
'''---------------------------------------------------------------
helper function: generate random QUALITY of given length
---------------------------------------------------------------'''
def rnd_qual( l ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    cmd = f"{tool} -f qual -l {l} -s 7"
    return try_cmd( cmd, "sam.py:rnd_qual()" )

'''---------------------------------------------------------------
helper function: transform cigar with given inserts into
a 'clean' cigar, bam-load does accept
---------------------------------------------------------------'''
def merge_cigar( cigar ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    cmd = f"{tool} -f cigar -c {cigar}"
    return try_cmd( cmd, "sam.py:merge_cigar()" )

'''---------------------------------------------------------------
helper function: to get length of a reference ( from the RefSeq-Acc )
---------------------------------------------------------------'''
def ref_len( ref ):
    tool = get_tool( "TESTBINDIR", "sampart" )
    cmd = f"{tool} -f rlen -r {ref}"
    return try_cmd( cmd, "sam.py:ref_len()" )

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
    cmd = f"{tool} {params} -o x_csra -k x.kfg x.sam 2>err.txt"
    txt1 = try_cmd( cmd, "sam.py:bam_load( bam-load )" )

    tool = get_tool( "BINDIR", "kar" )
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
preform a sra-sort on a given cSRA-file
    will create a temporary directory ( x_csra )
    performs sra-sort, and prints it's output
    kar's the created directory into the given output-file
    can be asked to keep the temporary files
    list ........ list of SAM-objects
    output....... name of cSRA-file to be created
    params....... parameters passed into bam-load
    keep_files... False/True for debugging temp. files
==============================================================='''
def sra_sort( input, output, params = "", keep_files = False ) :
    rm_dir( "s_csra" )
    rm_file( output )
    rm_file( "err.txt" )

    tool = get_tool( "BINDIR", "sra-sort" )
    cmd = f"{tool} {input} s_csra -f {params} 2>err.txt"
    txt1 = try_cmd( cmd, "sam.py:sra_sort( sra-sort )" )

    tool = get_tool( "BINDIR", "kar" )
    cmd = f"{tool} --create {output} -d s_csra -f 2>err.txt"
    txt2 = try_cmd( cmd, "sam.py:sra_sort( kar )" )
    if not keep_files :
        rm_dir( "s_csra" )
    print_txt_list( [ load_file( "err.txt" ), txt1, txt2 ] )
    rm_file( "err.txt" )
    return 1


def vdb_dump( accession, params = "" ) :
    try :
        tool = get_tool( "BINDIR", "vdb-dump" )
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
def make_prim( qname, flags, refname, refalias, pos, mapq, cigar, rnxt = "*", pnxt = "0" ) :
    return SAM( qname, flags | FLAG_PROPPER, refname, refalias, pos, mapq, merge_cigar( cigar ), 
        cigar2read( cigar, pos, refname ), rnxt, pnxt )


'''===============================================================
    make a secondary SAM-alignment
==============================================================='''
def make_sec( qname, flags, refname, refalias, pos, mapq, cigar, rnxt = "*", pnxt = "0" ) :
    return SAM( qname, flags | FLAG_SECONDARY, refname, refalias, pos, mapq, merge_cigar( cigar ), 
        cigar2read( cigar, pos, refname ), rnxt, pnxt )


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
        self.qual = rnd_qual( len( self.seq ) )
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

    def pair_with( self, other ) :
        self.nxt_ref = other.refalias
        self.nxt_pos = other.pos
        other.nxt_ref = self.refalias
        other.nxt_pos = self.pos
        self.flags |= FLAG_MULTI
        other.flags |= FLAG_MULTI
        self.set_flag( FLAG_FIRST, True )
        other.set_flag( FLAG_FIRST, False )
        self.set_flag( FLAG_LAST, False )
        other.set_flag( FLAG_LAST, True )
        self.set_flag( FLAG_NEXT_UNMAPPED, other.flags & FLAG_UNMAPPED )
        self.set_flag( FLAG_NEXT_REVERSED, other.flags & FLAG_REVERSED )
        other.set_flag( FLAG_NEXT_UNMAPPED, self.flags & FLAG_UNMAPPED )
        other.set_flag( FLAG_NEXT_REVERSED, self.flags & FLAG_REVERSED )
        other.qname = self.qname

    def link_to( self, other ) :
        self.flags |= FLAG_MULTI
        self.set_flag( FLAG_FIRST, other.flags & FLAG_FIRST )
        self.set_flag( FLAG_LAST, other.flags & FLAG_LAST )
        self.set_flag( FLAG_NEXT_UNMAPPED, other.flags & FLAG_UNMAPPED )
        self.set_flag( FLAG_NEXT_REVERSED, other.flags & FLAG_REVERSED )
        self.nxt_ref = other.refalias
        self.nxt_pos = other.pos
        self.qname = other.qname
