#!perl -w

use strict;
use warnings;

my $shuffle = 1;
my $rev = 1;
my $cmpl = 1;

for (@ARGV) {
    if (/no-shuffle/) {
        $shuffle = 0;
        next;
    }
    if (/no-rev/) {
        $rev = 0;
        next;
    }
    if (/no-cmpl/) {
        $cmpl = 0;
        next;
    }
    if (/help/) {
        print "$0 [no-shuffle] [no-rev] [no-cmpl] [help]\n";
        exit 0;
    }
    print "usage: $0 [no-shuffle] [no-rev] [no-cmpl] [help]\n";
    exit 1;
}

my @output;

while (defined($_ = <>)) {
    my $m = 0;
    
    if ($rev && rand() < 0.5) {
        $m |= 1
    }
    if ($cmpl && rand() < 0.5) {
        $m |= 2
    }
    
    if ($m) {
        my @F = \split /\t/;
        my $r = $F[0];
        if ($m & 1) {
            $$r = reverse $$r;
        }
        if ($m & 2) {
            $$r =~ tr/ACGT/TGCA/;
        }
    }
    if ($shuffle && @output) {
        my $i = int(rand(scalar(@output) + 1));
        ($_, $output[$i]) = ($output[$i], $_) if $i < scalar(@output);
    }
    push @output, $_;
}

print @output;
