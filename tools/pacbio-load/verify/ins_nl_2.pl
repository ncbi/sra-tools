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
    read ( F_in, $buffer, $line * 2 ); # read as many int16's as the line says...
    for ( $nr = 0; $nr < $line; $nr++ )
    {
        if ( $nr > 0 ) { print ", "; }
        # take 2 bytes (int16) from the buffer
        $intval = substr( $buffer, $nr * 2, 2 );
        print ( unpack( S, $intval ) ); # S ... unsigned int16
    }
    print "\n";
}
