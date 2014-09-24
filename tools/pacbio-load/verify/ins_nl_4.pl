#!/usr/bin/perl -w

my $buffer = "";
my $F_pos;
my $F_in;
my $nr;
my $intval;

open ( F_pos, $ARGV[1] ) or die "Could not open $ARGV[1]: $!\n";
open ( F_in, $ARGV[0] ) or die "Could not open $ARGV[0]: $!\n";
binmode( F_in );
foreach $line ( <F_pos> )
{
    chomp ( $line );  # remove the newline from $line.
    read ( F_in, $buffer, $line * 4 ); # read as many int32's as the line says...
    for ( $nr = 0; $nr < $line; $nr++ )
    {
        if ( $nr > 0 ) { print ", "; }
        # take 4 bytes (int32) from the buffer
        $intval = substr( $buffer, $nr * 4, 4 );
        print ( unpack( L, $intval ) ); # L ... unsigned int32
    }
    print "\n";
}
