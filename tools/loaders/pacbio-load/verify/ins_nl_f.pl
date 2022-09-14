#!/usr/bin/perl -w

my $buffer = "";
my $F_in;
my $nr;
my $floatbuf;
my $floatval;

open ( F_in, $ARGV[0] ) or die "Could not open $ARGV[0]: $!\n";
binmode( F_in );
while ( <F_in> )
{
    if ( read ( F_in, $buffer, 16 ) == 16 ) # read 16 bytes = 4 x float 32
    {
        print "[";
        for ( $nr = 0; $nr < 4; $nr++ )
        {
            if ( $nr > 0 ) { print ", "; }
            # take 4 bytes (float32) from the buffer
            $floatbuf = substr( $buffer, $nr * 4, 4 );
            $floatval = unpack( 'f', $floatbuf ); # f ... float int32-bit
            print $floatval;
        }
        print "]\n";
    }
}
close ( F_in );
