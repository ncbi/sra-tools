#!/usr/bin/perl
#============================================================================
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
# ===========================================================================

use IO::Handle;
use IO::File;
use IPC::Open2;
use FileHandle;

my $libname;
my $libvers;
my @files;

sub USAGE() {
  print "Usage: $0 -o <lib name> [-v <lib vers file>] <c source files>\n";
  exit 1;
}

sub file_contents($) {
  local $/;
  local $_ = shift;
  my $fh;

  $fh = new IO::File $_, 'r' or die "can't open $_ for read";
  undef $/;
  $_ = <$fh>;
  undef $fh;

  return $_;
}

sub process_args() {
  my $i;

  for ($i = 0; $i < scalar(@ARGV); ++$i) {
    $_ = $ARGV[$i];
    last unless /^-(.)/;
    if ($1 eq 'o') {
      $libname = $ARGV[++$i];
    }
    elsif ($1 eq 'v') {
      $_ = $ARGV[++$i];
      $_ = file_contents($_);
      chomp;
      warn "vers file is empty" if /^$/;
      if (/(\d+)\.(\d+)\.(\d+)/) {
        $libvers = "$1.$2";
      }
      else {
        warn "'$_' is not a valid version";
      }
    }
    else {
      USAGE;
    }
  }
  USAGE unless $libname;

  for (++$i; $i != scalar(@ARGV); ++$i) {
    push @files, $ARGV[$i];
  }
}

sub process_files() {
    foreach (@files) {
	my $in = new IO::File $_, 'r' or die "can't open file '$_' for read";
	{
	    local $_;

	    while ($_ = <$in>, defined($_)) {
		print "\t$1\n" if /^VTRANSFACT_IMPL\s*\(\s*(\w+)/;
	    }
	}
        $in->close();
    }
}

process_args;
USAGE unless $libname;
USAGE unless scalar(@files);

print "LIBRARY $libname\n";
print "VERSION $libvers\n" if $libvers;
print "EXPORTS\n";
process_files();

exit 0;
