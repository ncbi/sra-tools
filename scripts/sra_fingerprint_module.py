#!/usr/bin/env python3

import json, hashlib, subprocess, enum, os
import xml.parsers.expat

# ---------------------------------------------------------------------------------------

class Process :
    def __init__( self, cmd, ShellFlag=False ) :
        try :
            if ShellFlag :
                self . proc = subprocess . Popen( cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True )
            else :
                self . proc = subprocess . Popen( cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=False )
        except Exception as e :
            self . proc = None

    def result( self ) :
        if self . proc :
            output, _ = self . proc . communicate()
            if self . proc . returncode == 0 : return output . decode()
        return None

    def write_to( self, dst ) :
        if self . proc and dst . proc :
            while self . proc . stdout . readable() :
                line = self . proc . stdout . readline()
                if len( line ) == 0 :
                    break
                dst . proc . stdin . write( line )

# ---------------------------------------------------------------------------------------

def is_executable( toolname, option, binpath ) :
    tool = os . path . join( binpath, toolname )
    if binpath :
        if not os . path . isfile( tool ) : return False
        if not os . access( tool, os . X_OK ) : return False
    return not Process( [ tool, option ] ) . result() is None

# ---------------------------------------------------------------------------------------

class FingerprintProducer :
    def __init__( self, acc, binpath = "" ) :
        self . acc = acc
        self . binpath = binpath

    def produce( self ) :
        if self . acc is None or self . binpath is None : return None
        if not is_executable( "qa-stats", "-h", self . binpath ) : return None
        if not  is_executable( "vdb-dump", "-V", self . binpath ) : return None
        qa_stats = os . path . join( self . binpath, "qa-stats" )
        vdb_dump = os . path . join( self . binpath, "vdb-dump" )
        try :
            dst = Process( [ qa_stats, "-f" ] )
            columns = "READ,READ_LEN,READ_START,READ_TYPE,SPOT_GROUP"
            src = Process( [ vdb_dump, self . acc, "-f", "tab", "-C", columns ] )
            src . write_to( dst )
            return dst . result()
        except Exception as e :
            print( e )
        return None

# ---------------------------------------------------------------------------------------

class XmlExtractor :
    def __init__( self, txt ) :
        self . data = ""
        if txt is not None :
            p = xml.parsers.expat.ParserCreate()
            p . EndElementHandler = self . end_element
            p . CharacterDataHandler = self . char_data
            p . Parse( txt, 1 )

    def end_element( self, name ) :
        self . data = self . data . strip()

    def char_data( self, data ):
        self . data += data

# ---------------------------------------------------------------------------------------

class FingerprintExtractor :
    def __init__( self, acc, binpath = "" ) :
        self . acc = acc
        self . binpath = binpath

    def extract_meta( self, query ) :
        if self . binpath is None : return None
        if self . acc is None : return None
        if not is_executable( "kdbmeta", "-V", self . binpath ) : return None
        kdbmeta = os.path.join( self . binpath, "kdbmeta" )
        try :
            src = Process( [ kdbmeta, "-T", "SEQUENCE", self.acc, query ] )
            return src . result()
        except Exception as e :
            print( e )
        return None

    def extract( self ) :
        data   = self . extract_meta( "QC/current/fingerprint" )
        fp = XmlExtractor( data ) . data . strip( "'" )
        digest = self . extract_meta( "QC/current/digest" )
        di = XmlExtractor( digest ) . data . replace( "'", "\"" )
        return f"{{\n\"fingerprint\":{fp},\n\"fingerprint-digest\":{di}\n}}"

# ---------------------------------------------------------------------------------------

class Fingerprint :
    def __init__( self, data, src ) :
        try :
            self . content = data[ "fingerprint" ]
            self . digest = data[ "fingerprint-digest" ]
        except Exception as e :
            self . content = []
            self . digest = ""
        self . src = src

    def __str__( self ) :
        s = f"source  : {self . src}\n"
        s += f"digest  : {self . digest}\n"
        s += f"valid   : {self . check_digest()}\n"
        for base in [ 'A', 'C', 'G', 'T', 'N' ] :
            s += f"total {base} : {self . total( base )}\n"
        s += f"total   : {self . total_bases()}\n"
        return s

    @classmethod
    def fromFile( cls, filename ) :
        src = f"file:{filename}"
        with open( filename, "r" ) as f:
            return cls( json . load( f ), src )

    @classmethod
    def fromString( cls, s, src = "from string" ) :
        return cls( json . loads( s ), src )

    @classmethod
    def computeFromReadsOfAccession( cls, acc, binpath = "" ) :
        try :
            fpp = FingerprintProducer( acc, binpath )
            src = f"acc:{acc} (computed)"
            return cls( json . loads( fpp . produce() ), src )
        except Exception as e :
            print( e )
        return None

    @classmethod
    def extractFromMetaOfAccession( cls, acc, binpath = "" ) :
        try :
            fpe = FingerprintExtractor( acc, binpath )
            src = f"acc:{acc} (meta)"
            return cls( json . loads( fpe . extract() ), src )
        except Exception as e :
            print( e )
        return None

    @staticmethod
    def digest_from_string( s ) :
        h = hashlib.sha256()
        h . update( s . encode() )
        return h . hexdigest()

    def writeToFile( self, filename ) :
        with open( filename, "w" ) as f :
            f.write( "{\n" )
            f.write( f"    \"fingerprint\":{self.content},\n")
            f.write( f"    \"fingerprint-digest\":\"{self.digest}\"\n")
            f.write( "}" )

    def as_string( self ) :
        return f"{ self. content}".replace( " ", "" ).replace( "'", "\"" )

    def check_digest( self ) :
        return self . digest_from_string( self . as_string() ) == self . digest

    def maxpos( self ) :
        return self . content[ 'maximum-position' ]

    def total( self, key ) :
        return sum( self . content[ key ] )

    def total_bases( self ) :
        return sum( map( self . total, [ 'A', 'C', 'G', 'T', 'N' ] ) )

    def diff( self, other, key ) :
        return self . total( key ) - other . total( key )

    def total_diff( self, other ) :
        return sum( map( lambda a : self . diff( other, a ), [ 'A', 'C', 'G', 'T' ] ) )

    def redacted( self, other ) :
        if other is None : return False
        if not self . check_digest() : return False
        if not other . check_digest() : return False
        if self . digest == other . digest : return False
        if self . total_bases() != other . total_bases() : return False
        if self . maxpos() != other . maxpos() : return False
        if self . content[ "EoR" ] != other . content[ "EoR" ] : return False
        return self . total_diff( other ) == -self . diff( other, 'N' )

# ---------------------------------------------------------------------------------------

class FingerprintComp :
    def __init__( self, A, B ) :
        A_is_None = A is None
        B_is_None = B is None
        if A_is_None :
            self . A_src = "None"
            self . A_valid = False
        else :
            self . A_src = A . src
            self . A_valid = A . check_digest()
        if B_is_None :
            self . B_src = "None"
            self . B_valid = False
        else :
            self . B_src = B . src
            self . B_valid = B . check_digest()
        if A_is_None or B_is_None :
            self . equal_digest = False
            self . equal_EoR    = False
            self . equal_bases  = False
            self . equal_maxpos = False
            self . redacted = False
        else :
            self . equal_digest = A . digest == B . digest
            self . equal_EoR    = A . content[ "EoR" ]  == B . content[ "EoR" ]
            self . equal_bases  = A . total_bases() == B . total_bases()
            self . equal_maxpos = A . maxpos() == B . maxpos()
            self . redacted = A . redacted( B )

    def __str__( self ) :
        s  = f"A : {self . A_src}\n"
        s += f"B : {self . B_src}\n"
        s += f"A_valid : {self . A_valid}\n"
        s += f"B_valid : {self . B_valid}\n"
        s += f"equal digest : {self . equal_digest}\n"
        s += f"equal EoR    : {self . equal_EoR}\n"
        s += f"equal bases  : {self . equal_bases}\n"
        s += f"equal maxpos : {self . equal_maxpos}\n"
        s += f"redacted     : {self . redacted}\n"
        return s

# ---------------------------------------------------------------------------------------
def AccessionTypeOf( acc, binpath = "" ) :
        if acc is None : return None
        if binpath is None : return None
        if not is_executable( "vdb-dump", "-V", binpath ) : return None
        vdbdump = os.path.join( binpath, "vdb-dump" )
        tables = Process( [ vdbdump, acc, "-E" ] ) . result()
        if tables :
            if tables . find( "cannot enum tables" ) == 0 :
                return "legacy"
            elif tables . find( "SEQUENCE" ) > 0 :
                if tables . find( "PRIMARY_ALIGNMENT" ) > 0 :
                    return "cSRA"
                else :
                    return "standard"
        return None

# ---------------------------------------------------------------------------------------

#this is needed because the FingerprintInserter requires an "unkar'd" accession in
#a writable state... and kar only works on files - not directories
def PrepareForFingerprintInsertFromFile( src, dst, rows, binpath = "" ) :
    if not src . endswith( ".sra" ) : return None
    if not is_executable( "vdb-copy", "-V", binpath ) : return None
    vdbcopy = os.path.join( binpath, "vdb-copy" )
    cmd = [ vdbcopy, src, dst, '-R', rows '-f' ]
    s = Process( cmd, False ) . result()
    print( f"result={s}" )


def PrepareForFingerprintInsert( src, dst, rows, binpath = "" ) :
    if src is None : return None
    if dst is None : return None
    if rows is None : return None
    if binpath is None : return None
    if os.path.isfile( src ) :
        return PrepareForFingerprintInsertFromFile( src, dst, rows, binpath )
    if os.path.isdir( src ) :
        hd, tl = os.path.split( src )
        fname = os.path.join( src, tl ) + ".sra"
        if os.path.isfile( fname ) :
            return PrepareForFingerprintInsertFromFile( fname, dst, rows, binpath )
    return None

# ---------------------------------------------------------------------------------------

class FingerprintInserter :
    def __init__( self, acc, binpath = "" ) :
        self . acc = acc
        self . binpath = binpath

    def run( self ) :
        if self . acc is None : return False
        if self . binpath is None : return False
        at = AccessionTypeOf( self . acc, self . binpath )
        if at != "standard" : return False
        fp = Fingerprint . computeFromReadsOfAccession( self . acc, self . binpath )
        if not fp : return False
        print( fp )
        return False
        if not is_executable( "kdbmeta", "-V", self . binpath ) : return False
        kdbmeta = os.path.join( self . binpath, "kdbmeta" )
        p1 = Process( [ kdbmeta, "-TSEQUENCE", self . acc, f"QC/current/digest={ fp . digest }" ] )
        r1 = p1 . result()
        print( f"r1=>{r1}<" )
        if not r1 : return False
        p2 = Process( [ kdbmeta, "-TSEQUENCE", self . acc, f"QC/current/fingerprint={ fp . content }" ] )
        r2 = p2 . result()
        print( f"r1=>{r2}<" )
        return r2 != None

# ---------------------------------------------------------------------------------------

class FingerprintTests :
    def __init__( self, acc, binpath = "" ) :
        self . acc = acc
        self . binpath = binpath
        self . tests = [ self . test1, self . test2, self . test3, self . test4,
                        self . test5, self . test6 ]

    def run( self, test_numbers ) :
        for n in test_numbers :
            if n <= len( self . tests ) :
                print( f"Test #{ n }" )
                self . tests[ n - 1 ]()

    def runall( self ) :
        n = 1
        for test in self . tests :
            print( f"Test #{ n }" )
            test()
            n = n + 1

    # test if we can compute the fingerprint from the stored bases ( expensive! )
    def test1( self ) :
        print( Fingerprint . computeFromReadsOfAccession( self . acc ) )

    # test if we can extract the fingerprint from the metatdata
    def test2( self ) :
        print( Fingerprint . extractFromMetaOfAccession( self . acc ) )

    # test if we can compare the computed vs. the extracted fingerprint
    def test3( self ) :
        fp1 = Fingerprint . extractFromMetaOfAccession( self . acc )
        fp2 = Fingerprint . computeFromReadsOfAccession( self . acc )
        print( FingerprintComp( fp1, fp2 ) )

    # test if we can detect redaction of bases between 2 fingerprints
    def test4( self ) :
        fp1 = Fingerprint . extractFromMetaOfAccession( self . acc, self . binpath )
        fp2 = Fingerprint . computeFromReadsOfAccession( self . acc, self . binpath )
        fp2 . content[ 'A' ][ 1 ] = fp2 . content[ 'A' ][ 1 ] - 1
        fp2 . content[ 'N' ][ 1 ] = fp2 . content[ 'N' ][ 1 ] + 1
        fp2 . digest = Fingerprint . digest_from_string( fp2 . as_string() )
        fc = FingerprintComp( fp1, fp2 )
        print( fc )

    # test if we can safely compare a fingerprint against None
    def test5( self ) :
        fp1 = Fingerprint . extractFromMetaOfAccession( self . acc )
        fc = FingerprintComp( fp1, None )
        print( fc )

    # test if we can supply a path to the binaries instead of relying on them
    # beeing in the search-path
    def test6( self ) :
        print( Fingerprint . computeFromReadsOfAccession( self . acc, self . binpath ) )


if __name__ == "__main__":
    pass
