#!/usr/bin/env python3

import json, hashlib, subprocess, enum, os
import xml.parsers.expat

# ---------------------------------------------------------------------------------------

class Process :
    def __init__( self, cmd ) :
        self . proc = subprocess.Popen( cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE )

    def result( self ) :
        output, _ = self . proc . communicate()
        return output . decode()

    def write_to( self, dst ) :
        while self . proc . stdout . readable() :
            line = self . proc . stdout . readline()
            if len( line ) == 0 :
                break
            dst . proc . stdin . write( line )

# ---------------------------------------------------------------------------------------

def is_executable( p ) :
    return os . path . isfile( p ) and os . access( p, os . X_OK )

# ---------------------------------------------------------------------------------------

class FingerprintProducer :
    def __init__( self, acc, binpath = "" ) :
        self . acc = acc
        self . binpath = binpath

    def produce( self ) :
        if self . acc is None or self . binpath is None :
            return None
        qa_stats = os . path . join( self . binpath, "qa-stats" )
        vdb_dump = os . path . join( self . binpath, "vdb-dump" )
        found = True
        if self . binpath :
            found = is_executable( qa_stats ) and is_executable( vdb_dump )
        if found :
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
        if self . binpath is None :
            return None
        if self . acc is None :
            return None
        kdbmeta = os.path.join( self . binpath, "kdbmeta" )
        found = True
        if self . binpath :
            found = is_executable( kdbmeta )
        if found :
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
        if other is None :
            return False
        if not self . check_digest() :
            return False
        if not other . check_digest() :
            return False
        if self . digest == other . digest :
            return False
        if self . total_bases() != other . total_bases() :
            return False
        if self . maxpos() != other . maxpos() :
            return False
        if self . content[ "EoR" ] != other . content[ "EoR" ] :
            return False
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

class FingerprintInsert :
    def __init__( self, acc, binpath = "" ) :
        self . acc = acc
        self . binpath = binpath

    def run() :
        if self . acc is None :
            return False
        if self . binpath is None :
            return False


# ---------------------------------------------------------------------------------------

if __name__ == "__main__":
    pass
