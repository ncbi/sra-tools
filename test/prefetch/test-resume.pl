#!/usr/local/bin/perl -w
use strict;

my $verbose; # = 1;
my $ALL = 1;
my $RUN = 1;

my %tests;

my $ACC = 'SRR053325';

my $cmd_verb = '-vv';
$cmd_verb = '-v';
$cmd_verb = '';
my $cmd = "prefetch $ACC $cmd_verb";

my $out;

unless (-e "$ACC.sra.org" && -e "$ACC") { # save orig run
  $out = `prefetch $ACC -fy`; print $out if $verbose; die if $?; 
  `mv $ACC/$ACC.sra $ACC.sra.org`; die if $?;
} else { `rm $ACC/$ACC.sra*`; }

if ($ALL || $tests{'no trans file'}) {
print "======\nno trans file, download is complete: start from 0\n" if $verbose;
`cp $ACC.sra.org $ACC/$ACC.sra.tmp`; die if $?;
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{'short magic'}) {
print "=====\nshort magic, download started: start from 0\n" if $verbose;
`head -26 $ACC.sra.org > $ACC/$ACC.sra.tmp`; die if $?;
`printf 'NCBIprT' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?;
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{'bad magic'}) {
print "=====\nbad magic, download started: start from 1\n" if $verbose;
`head -26 $ACC.sra.org > $ACC/$ACC.sra.tmp`; die if $?;
`printf 'NCBIprfr\n1234\n' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?;
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{'trans file empty'}) {
print "=====\ntrans file empty, download started: start from 0\n" if $verbose;
`head -26 $ACC.sra.org > $ACC/$ACC.sra.tmp`; die if $?;
`printf 'NCBIprTr\n' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?;
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{'trans file smaller/1'}) {
print "=====\ntrans file smaller/1, download started: start from 1\n" if $verbose;
`head -26 $ACC.sra.org > $ACC/$ACC.sra.tmp`; die if $?;
`printf 'NCBIprTr\n1234\n' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?;
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{'trans file smaller/2'}) {
print "=====\ntrans file smaller/2, download started: start from 2\n" if $verbose;
`head -26 $ACC.sra.org > $ACC/$ACC.sra.tmp`; die if $?;
`printf 'NCBIprTr\n1234\n2048\n' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?;
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{'trans file bigger/3'}) {
print "=====\ntrans file bigger/3, download started: start from 2\n" if $verbose;
`head -26 $ACC.sra.org > $ACC/$ACC.sra.tmp`; die if $?;
`printf 'NCBIprTr\n1234\n2048\n7689\n' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?;
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{'trans file equals'}) {
print "=====\ntrans file smaller/1, download started: start from 1\n" if $verbose;
`head -26 $ACC.sra.org > $ACC/$ACC.sra.tmp`; die if $?;
`printf 'NCBIprTr\n5512\n' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?;
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{'no download'}) {
print "===\ntrans file complete, download is empty: start from 0\n" if $verbose;
`printf 'NCBIprTr\n31838\n' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?; 
$cmd = "prefetch $ACC $cmd_verb -fy";
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

if ($ALL || $tests{"don't resume"}) {
print "=====\ntrans file smaller/1, download started: start from 1\n" if $verbose;
`head -26 $ACC.sra.org > $ACC/$ACC.sra.tmp`; die if $?;
`printf 'NCBIprTr\n5512\n' > $ACC/$ACC.sra.prt`; die if $?;
$out = `prefetch $ACC/$ACC.sra.prt`; print $out if $verbose;die if $?;
$cmd = "prefetch $ACC $cmd_verb -rn";
if ($RUN) {
  $out = `$cmd`; print $out if $verbose; die if $?; 
  `diff $ACC.sra.org $ACC/$ACC.sra`; die if $?;
  `rm $ACC/$ACC.sra`; die if $?;
}
}

unless ($RUN) {
 print "$cmd\n\n" . `ls -l $ACC` . "\n";
 print `cat $ACC/$ACC.sra.prt`. "\n" . `hexdump -C $ACC/$ACC.sra.prf`;
} # =pod =cut