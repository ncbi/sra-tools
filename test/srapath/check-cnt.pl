use strict;

die unless ( $#ARGV == -1 );

$_ = <>;
chomp;

my $e = 1;
if (/^1$/) {
    $e = 0;
}
else {
    print STDERR "Error: unexpected number of output items\n";
}

exit $e;
