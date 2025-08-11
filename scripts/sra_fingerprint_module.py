#!/usr/bin/env python3

import json, hashlib, subprocess, enum, os, sys
import xml.parsers.expat

# ---------------------------------------------------------------------------------------

class Process :
    def __init__( self, cmd ) :
        try :
            self . proc = subprocess . Popen( cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, shell=False )
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

    def extract_meta_node( self, query ) :
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

    def extract( self, metapath ) :
        data   = self . extract_meta_node( f"{metapath}fingerprint" )
        if data is None :
            return None
        fp = XmlExtractor( data ) . data . strip( "'" )
        digest = self . extract_meta_node( f"{metapath}digest" )
        if digest is None :
            return None
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

    def __add__( self, other ) :
        if not isinstance( other, Fingerprint ) :
            return NotImplemented
        fp = dict( self . content )
        for key in [ 'A', 'C', 'G', 'T', 'N' ] :
            fp[ key ] = [ fp[ key ][ i ] + other . content[ key][ i ] for i in range( len( fp[ key ] ) ) ]
        data = { "fingerprint" : fp, "fingerprint-digest" : self . digest  }
        res = Fingerprint( data, self . src )
        res . update_digest()
        return res

    def __sub__( self, other ) :
        if not isinstance( other, Fingerprint ) :
            return NotImplemented
        fp = dict( self . content )
        for key in [ 'A', 'C', 'G', 'T', 'N' ] :
            fp[ key ] = [ fp[ key ][ i ] - other . content[ key][ i ] for i in range( len( fp[ key ] ) ) ]
        data = { "fingerprint" : fp, "fingerprint-digest" : self . digest  }
        res = Fingerprint( data, self . src )
        res . update_digest()
        return res

    def __eq__( self, other ) :
        if not isinstance( other, Fingerprint ) : return NotImplemented
        if not self . check_digest() : return False
        if not other . check_digest() : return False
        return self . digest == other . digest

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
            return cls( json . loads( fpp . produce() ), f"acc:{acc} (computed)" )
        except Exception as e :
            print( f"Fingerprint.computeFromReadsOfAccession():{e}" )
        return None

    @classmethod
    def extractFromMetaOfAccession( cls, acc, metapath = "QC/current/", binpath = "" ) :
        try :
            fpe = FingerprintExtractor( acc, binpath )
            fp  = fpe . extract( metapath )
            if fp is None :
                return None
            return cls( json . loads( fp ), f"acc:{acc} (meta)" )
        except Exception as e :
            print( f"Fingerprint.extractFromMetaOfAccession():{e}" )
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

    def update_digest( self ) :
        self . digest = self . digest_from_string( self . as_string() )

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
        else :
            self . equal_digest = A . digest == B . digest
            self . equal_EoR    = A . content[ "EoR" ]  == B . content[ "EoR" ]
            self . equal_bases  = A . total_bases() == B . total_bases()
            self . equal_maxpos = A . maxpos() == B . maxpos()

    def __str__( self ) :
        s  = f"A : {self . A_src}\n"
        s += f"B : {self . B_src}\n"
        s += f"A_valid : {self . A_valid}\n"
        s += f"B_valid : {self . B_valid}\n"
        s += f"equal digest : {self . equal_digest}\n"
        s += f"equal EoR    : {self . equal_EoR}\n"
        s += f"equal bases  : {self . equal_bases}\n"
        s += f"equal maxpos : {self . equal_maxpos}\n"
        return s

# ---------------------------------------------------------------------------------------

class FingerprintTests :
    def __init__( self, acc, binpath = "" ) :
        self . acc = acc
        self . binpath = binpath
        self . tests = [ self . test1, self . test2, self . test3, self . test4,
                        self . test5 ]

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

    # test if we can safely compare a fingerprint against None
    def test4( self ) :
        fp1 = Fingerprint . extractFromMetaOfAccession( self . acc )
        fc = FingerprintComp( fp1, None )
        print( fc )

    # test if we can supply a path to the binaries instead of relying on them
    # beeing in the search-path
    def test5( self ) :
        print( Fingerprint . computeFromReadsOfAccession( self . acc, self . binpath ) )

# ---------------------------------------------------------------------------------------
def QCCHECK( acc ) :
    #compare fingerprint stored in metadata against fingerprint computed from bases
    from_meta = Fingerprint . extractFromMetaOfAccession( acc )
    from_data = Fingerprint . computeFromReadsOfAccession( acc )
    print( f"simple comparison: { from_meta == from_data }" )
    print( FingerprintComp( from_meta, from_data ) )

    #example of verifying the history audit
    event1 = "QC/history/event_1/"
    hist_orig = Fingerprint . extractFromMetaOfAccession( acc, f"{event1}original/" )
    if not hist_orig is None :
        hist_added   = Fingerprint . extractFromMetaOfAccession( acc, f"{event1}added/" )
        hist_removed = Fingerprint . extractFromMetaOfAccession( acc, f"{event1}removed/" )
        res = hist_orig + hist_added - hist_removed
        res . src += " ... history audit"
        print( FingerprintComp( from_meta, res ) )

if __name__ == "__main__":
    if len( sys.argv ) > 1 :
        QCCHECK( sys.argv[ 1 ] )
