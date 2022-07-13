use strict;

my $found = 0;

foreach (<STDIN>){
  chomp;
  if ( $_ ne "" ) {
    next if ( /blastn/ );
    unless ( /$ARGV[0]/ ) {
      die "*** bad version $_ (expected $ARGV[0])";
    } else {
      ++ $found;
    }
  }
}

die "*** bad version (expected $ARGV[0])" unless ( $found );
