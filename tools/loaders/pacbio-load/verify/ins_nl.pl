#!/usr/bin/perl -w

my $buffer = "";
my $F_pos;
my $F_in;

open ( F_pos, $ARGV[1] ) or die "Could not open $ARGV[1]: $!\n";
open ( F_in, $ARGV[0] ) or die "Could not open $ARGV[0]: $!\n";
binmode( F_in );
foreach $line ( <F_pos> )
{
    chomp ( $line );  # remove the newline from $line.
    read ( F_in, $buffer, $line ); # read as many bytes as the line says...
    print "$buffer\n";
}
