#!/usr/local/bin/perl -w
use strict;

my ($exit, $verb, $cmd) = @ARGV;
die unless defined $cmd;

# This script executes prefetch $cmd;
# expects it to succeed or fail according to $exit.
# $verb turns on verbosity.

# The scripts is used to test the order of downloads.
# It accepts list of strings on STDIN
# and expects them to be present in the same order in prefetch's output.

my @out = `$cmd -v 2>&1`;
print '$? = ' . "$?\n";
die "Unexpected failure while running:\n'$cmd':\n@out" if ($? && ! $exit);
die if (! $? && $exit);
print "'$cmd'\n" if ($verb);
print "@out" if ($verb);

@_ = <STDIN>;
my ($first, $last) = (0, $#_);

foreach (@out) {
  last if ($last < $first);

  print if ($verb > 1);

  my $fnd;
  for (my $i = $first; $i <= $last; ++$i) {
    my $ptrn = $_[$i];
    chomp $ptrn;
    my $re = qr/$ptrn/;
    if (/$re/) {
      if ($i == $first) {
        die "'$ptrn' and '$fnd' found together\n" if ($fnd);
        $fnd = $ptrn;
        print "$i: '$ptrn' found\n" if ($verb);
      } else {
        die "'$ptrn' found before '$_[$first]'\n";
      }
    }
  }

  ++$first if ($fnd);
}

die "'$_[$first]' not found" if ($first <= $last);
