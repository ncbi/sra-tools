use strict;

my $found = 0;

foreach (<STDIN>){
  print;
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
print "\n";
die "*** bad version (expected $ARGV[0])" unless ( $found );
