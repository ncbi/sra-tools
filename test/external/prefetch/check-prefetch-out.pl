#!/usr/local/bin/perl -w

# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ==============================================================================

use strict;

my ($exit, $verb, $cmd) = @ARGV;
die unless defined $cmd;

# This script executes prefetch $cmd;
# expects it to succeed or fail according to $exit.
# $verb turns on verbosity.

# The scripts is used to test the order of downloads.
# It accepts list of strings on STDIN
# and expects them to be present in the same order in prefetch's output.

my @out = `$cmd 2>&1`;
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
