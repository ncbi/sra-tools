#!/usr/bin/perl -w

my $buffer = "";
my $F_pos;
my $F_in;
my $firstvalue;

open ( F_pos, $ARGV[1] ) or die "Could not open $ARGV[1]: $!\n";
open ( F_in, $ARGV[0] ) or die "Could not open $ARGV[0]: $!\n";
binmode( F_in );
foreach $line ( <F_pos> )
{
    chomp ( $line );  # remove the newline from $line.
    read ( F_in, $buffer, $line ); # read as many bytes as the line says...
    @charbuf = split( //, $buffer ); # split the buffer into an array of char's
    $firstvalue = 1;
    foreach ( @charbuf )
    {
        if ( $firstvalue == 1 )
        {
            printf ( "%d", ord( $_ ) );
            $firstvalue = 0;
        }
        else
        {
            printf ( ", %d", ord( $_ ) );
        }
    }
    print "\n";
}
