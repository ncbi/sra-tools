import subprocess
import os
import shutil

'''---------------------------------------------------------------
helper function: generate READ from cigar,refname,refpos
---------------------------------------------------------------'''
def cigar2read( cigar, pos, ref ):
    cmd = "sampart -f read -c %s -p %d -r %s"%( cigar, pos, ref )
    return subprocess.check_output( cmd, shell = True )

'''---------------------------------------------------------------
helper function: generate random QUALITY of given length
---------------------------------------------------------------'''
def rnd_qual( l ):
    cmd = "sampart -f qual -l %d -s 7"%( l )
    return subprocess.check_output( cmd, shell = True )

'''---------------------------------------------------------------
helper function: transform cigar with given inserts into
a 'clean' cigar, bam-load does accept
---------------------------------------------------------------'''
def merge_cigar( cigar ):
    cmd = "sampart -f cigar -c %s"%( cigar )
    return subprocess.check_output( cmd, shell = True )

'''---------------------------------------------------------------
helper function: to get length of a reference ( from the RefSeq-Acc )
---------------------------------------------------------------'''
def ref_len( ref ):
    cmd = "sampart -f rlen -r %s"%( ref )
    return int( subprocess.check_output( cmd, shell = True ) )

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
    try :
        rm_dir( "x_csra" )
        rm_file( output )
        save_sam( list, "x.sam" )
        save_config( list, "x.kfg" )
        cmd = "bam-load %s -o x_csra -k x.kfg x.sam"%( params )
        txt = subprocess.check_output( cmd, stderr=subprocess.STDOUT, shell=True )
        print txt
        cmd = "kar --create %s -d x_csra -f"%( output )
        lr = subprocess.check_call( cmd, shell=True )
        if lr == 0 :
            if not keep_files :
                rm_dir( "x_csra" )
                rm_file( "x.sam" )
                rm_file( "x.kfg" )
            return 1
    except :
        pass
    return 0


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
    try :
        rm_dir( "s_csra" )
        rm_file( output )
        cmd = "sra-sort %s s_csra -f %s"%( input, params )
        txt = subprocess.check_output( cmd, stderr=subprocess.STDOUT, shell=True )
        print txt
        cmd = "kar --create %s -d s_csra -f"%( output )
        lr = subprocess.check_call( cmd, shell=True )
        if lr == 0 :
            if not keep_files :
                rm_dir( "s_csra" )
            return 1
    except :
        pass
    return 0

def vdb_dump( accession, params = "" ) :
    try :
        cmd = "vdb-dump %s %s"%( accession, params )
        txt = subprocess.check_output( cmd, stderr=subprocess.STDOUT, shell=True )
        print txt
        return 1
    except :
        pass
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
def make_prim( qname, flags, refname, refalias, pos, mapq, cigar, rnxt = "-", pnxt = "0" ) :
    return SAM( qname, flags | FLAG_PROPPER, refname, refalias, pos, mapq, merge_cigar( cigar ), 
        cigar2read( cigar, pos, refname ), rnxt, pnxt )


'''===============================================================
    make a secondary SAM-alignment
==============================================================='''
def make_sec( qname, flags, refname, refalias, pos, mapq, cigar, rnxt = "-", pnxt = "0" ) :
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
        res.append( "@SQ\tSN:%s\tAS:%s\tLN:%d"%( k, k, l ) )
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
            res.append( "%s\t%s"%( k, v ) )
    return res

'''---------------------------------------------------------------
helper function: save config file created from list of SAM-objects
    used in bam_load
---------------------------------------------------------------'''
def save_config( list, filename ) :
    with open( filename, "w" ) as f:
        for s in produce_config( list ) :
            f.write( "%s\n"%( s ) )

'''---------------------------------------------------------------
helper function: prints a list of SAM-objects
---------------------------------------------------------------'''
def print_sam( list ):
    for s in extract_headers( list ) :
        print s
    for s in list :
        print s

'''---------------------------------------------------------------
helper function: save a list of SAM-objects as file
    used in bam_load
---------------------------------------------------------------'''
def save_sam( list, filename ) :
    with open( filename, "w" ) as f:
        for s in extract_headers( list ) :
            f.write( "%s\n"%( s ) )
        for s in list :
            f.write( "%s\n"%( s ) )

'''===============================================================
    SAM-object
==============================================================='''
class SAM:

    def __init__( self, qname, flags, refname, refalias, pos, mapq, cigar, seq, rnxt, pnxt ) :
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

    def __str__( self ):
        return "%s\t%d\t%s\t%s\t%d\t%s\t%s\t%s\t%d\t%s\t%s"%( self.qname,
            self.flags, self.refalias, self.pos, self.mapq, self.cigar, self.nxt_ref,
            self.nxt_pos, self.tlen, self.seq, self.qual )    

    def set_flag( self, flagbit, state ) :
        if state :
            self.flags |= flagbit
        else :
            self.flags &= ~flagbit

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
