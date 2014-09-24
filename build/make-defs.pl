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
my $GCC = '';
my @files;

sub USAGE() {
  print "Usage: $0 -o <lib name> [-v <lib vers file>] <gcc spec> -- <header files>\n";
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
#  USAGE unless $libname;

  for ( ; $i != scalar(@ARGV); ++$i) {
    $_ = $ARGV[$i];
    last if $_ eq '--';
    $GCC .= "$_ ";
  }

  for (++$i; $i != scalar(@ARGV); ++$i) {
    push @files, $ARGV[$i];
  }
}

sub next_token($) {
  local $_;
  my $src = shift;

  if ($src =~ /^(\s*)/) {
    $src = substr $src, length($1);
  }
  if ($src =~ /^([^ ]+)/) {
    $_ = $1;
    if (/^([A-Za-z_][A-Za-z0-9_]+)/) {
      $_ = $1;
      $src = substr $src, length($_);
      return ($src, 'ident', $_);
    }
    else {
      $_ = substr $src, 0, 1;
      $src = substr $src, 1;
      return ($src, 'unknown', $_);
    }
  }
  return ($src, undef, undef);
}

my $id;
my $pt_st = 0;
my $visibility = 'extern';

sub process_token($$) {
  my $type = shift;
  my $tok = shift;

  if ($tok eq ';' || $tok eq ',') {
    if (($pt_st == 4 || $pt_st == 5) && $visibility eq 'extern') {
      if ($id) {
        print "\t$id\n";
      }
    }
    $id = undef;
    if ($tok eq ';') {
      $pt_st = 0;
      $visibility = 'extern';
    }
    return;
  }
  if ($pt_st == 0) {
    unless ($type eq 'ident') {
      $pt_st = 1;
      return;
    }
    if ($tok eq 'extern' || $tok eq 'static') {
      $visibility = $tok;
    }
    elsif ($tok eq 'typedef') {
      $pt_st = 1;
    }
    else {
      if ($tok eq 'const' || $tok eq 'volatile') {
        $pt_st = 2;
      }
      else {
        $pt_st = ($tok eq 'struct' || $tok eq 'class' || $tok eq 'union' || $tok eq 'enum') ? 3 : 4;
      }
    }
    return;
  }
  if ($pt_st == 1) {
    return;
  }
  if ($pt_st == 2) {
    unless ($type eq 'ident') {
      $pt_st = 1;
      return;
    }
    $pt_st = ($tok eq 'struct' || $tok eq 'class' || $tok eq 'union' || $tok eq 'enum') ? 3 : 4;
    return;
  }
  if ($pt_st == 3) {
    unless ($type eq 'ident') {
      $pt_st = 1;
      return;
    }
    $pt_st = 4;
    return;
  }
  if ($pt_st == 4) {
    if ($tok eq '__attribute__') {
      $pt_st = 5;
      return;
    }
    $id = $tok if $type eq 'ident';
    return;
  }
  if ($pt_st == 5) {
    return;
  }
  warn 'unpossible';
}

my $level = 0;

sub parse($$);
sub parse($$) {
  my $src = shift;
  my $end = shift;
  my $tok;
  my $type;

  ++$level;
  while (($src, $type, $tok) = next_token($src), defined($tok)) {
    last if ($tok eq $end);
    process_token($type, $tok) if ($level == 1);
    $src = parse($src, ')') if ($tok eq '(');
    $src = parse($src, '}') if ($tok eq '{');
    $src = parse($src, ']') if ($tok eq '[');
  }
  --$level;
  return $src;
}

sub process_files() {
  my $gcc_out;
  my $gcc_in;
  my $src = '';

  open2($gcc_in, $gcc_out, "$GCC") or die "can't run preprocessor $GCC";
  foreach (@files) {
    next unless $_;
    $_ = file_contents($_);
    next unless $_;
    s/^\#include\s.+$//gm unless /weak\.h/;
    $gcc_out->print($_);
  }

  undef $gcc_out; # else gcc won't produce output

  while ($_ = <$gcc_in>, defined($_)) {
    next if /^\s*$/;
    next if /^#/;
    chomp;
    $src .= " $_";
  }
  return $src;
}

process_args;
USAGE unless $GCC;
USAGE unless scalar(@files);

if (defined $libname) { 
    print "LIBRARY $libname\n";
    print "VERSION $libvers\n" if $libvers;
    print "EXPORTS\n"; 
    }
parse(process_files(), '');

exit 0;
