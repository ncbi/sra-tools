#!perl -w

use strict;
use warnings;

my @output;

while (defined($_ = <>)) {
    if (/^#/) {
        print;
        next;
    }
    my $i = rand(1 + scalar @output);
    if ($i == scalar @output) {
        push @output, $_;
    }
    else {
        push @output, $output[$i];
        $output[$i] = $_;
    }
}

print @output;
