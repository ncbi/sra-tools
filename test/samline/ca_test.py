import subprocess

#calls to sampart-binary for creating sequence, random quality, merged cigar-str, lenght of reference
def cigar2read( cigar, pos, ref ):
    cmd = "sampart -f read -c %s - p %d -r %s"%( cigar, pos, ref )
    return subprocess.check_output( cmd, shell = True )

def rnd_qual( l ):
    cmd = "sampart -f qual -l %d -s 7"%( l )
    return subprocess.check_output( cmd, shell = True )

def merge_cigar( cigar ):
    cmd = "sampart -f cigar -c %s"%( cigar )
    return subprocess.check_output( cmd, shell = True )


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

class SAM:
    def __init__( self, qname, flags, refname, refalias, pos, mapq, cigar ):
        self.qname = qname
        self.flags = flags
        self.refalias = refalias
        self.pos = pos
        self.mapq = mapq
        self.cigar = merge_cigar( cigar )
        self.seq = cigar2read( cigar, pos, refname )
        self.qual = rnd_qual( len( self.seq ) )
        self.nxt_ref = "*"
        self.nxt_pos = "*"
        self.tlen = 0

    def __str__( self ):
        return "%s\t%d\t%s\t%s\t%d\t%s\t%s\t%s\t%d\t%s\t%s"%( self.qname, self.flags, self.refalias,
        self.pos, self.mapq, self.cigar, self.nxt_ref, self.nxt_pos, self.tlen, self.seq, self.qual )    

    @classmethod
    def pair( cls, first, next ):
        first.nxt_ref = next.refalias
        first.nxt_pos = next.pos
        first.flags |= FLAG_MULTI
        first.flags |= FLAG_FIRST
        if next.flags & FLAG_UNMAPPED:
            first.flags |= FLAG_NEXT_UNMAPPED
        if next.flags & FLAG_REVERSED:
            first.flags |= FLAG_NEXT_REVERSED

        next.nxt_ref = first.refalias
        next.nxt_pos = first.pos
        next.flags |= FLAG_MULTI
        next.flags |= FLAG_LAST
        if first.flags & FLAG_UNMAPPED:
            next.flags |= FLAG_NEXT_UNMAPPED
        if first.flags & FLAG_REVERSED:
            next.flags |= FLAG_NEXT_REVERSED

        end = next.pos + len( next.seq )
        first.tlen = ( end - first.pos );
        next.tlen = -first.tlen

    @classmethod
    def headers( cls, list ):
        hdr = "@HD\tVN:1.3\n"
        hdr += "@SQ\tSN:%s\tAS:%s\tLN:%d\n"%( list[0].refalias, list[1].refalias, 1234567 )
        return hdr

REF  = "NC_011752.1"

SAMS = []
A1 = SAM( "A1", FLAG_PROPPER, REF, "c1", 17000, 20, "60M" )
A2 = SAM( "A2", FLAG_PROPPER, REF, "c1", 17500, 20, "50M" )
SAM.pair( A1, A2 )
SAMS.append( A1 )
SAMS.append( A2 )

print SAM.headers( SAMS )
for elem in SAMS:
    print elem

