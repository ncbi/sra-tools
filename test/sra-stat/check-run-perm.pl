use strict;

die if $#ARGV < 1;
my $srapath = $ARGV[0];
my $run     = $ARGV[1];

my $path = `$srapath $run 2> /dev/null`;
chomp $path;
#print "$path\n";

my $r = -r "$path";
unless ( $r ) {
    exit 1
} else {
    exit 0;
}
