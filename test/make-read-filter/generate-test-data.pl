#!/usr/bin/perl -w

use warnings;
use strict;

# NOTE: Rules for filtering
#  Quote:
#      Reads that have more than half of quality score values <20 will be
#      flagged ‘reject’.
#      Reads that begin or end with a run of more than 10 quality scores <20
#      are also flagged ‘reject’.
# 

sub rand_read($)
{
    my $y = '';
    $y .= substr("ACGT", int(rand(4)), 1) for (1..$_[0]);
    $y
}

sub rand_phred($$)
{
    return int(rand($_[1] - $_[0]))+$_[0];
}

sub rand_qual($$)
{
    my @y = map { rand_phred(20, 41) } (1..$_[0]);
    my $low = 0;
    if ($_[1] =~ /front/) {
        $low += 10;
        splice @y, 0, 10, map { rand_phred(2, 20) } (1..10)
    }
    if ($_[1] =~ /back/) {
        $low += 10;
        splice @y, -10, 10, map { rand_phred(2, 20) } (1..10)
    }
    if ($_[1] =~ /low/) {
        my $threshold = $_[0] / 2.0;
        my $need = 0;
        $need += 1 while ($need + $low <= $threshold);
        if ($need) {
            splice @y, -$need, $need;
            while ($need) {
                my $i = int(rand(scalar(@y)));
                splice @y, $i, 0, rand_phred(2, 20);
                $need -= 1;
            }
        }
    }
    join('', map { chr($_+33) } @y)
}

sub selftest() {
    my $qstr;
    my $n;
    
    $qstr = rand_qual(50, '');
    die "wrong length" unless length($qstr) == 50;
    $qstr = rand_qual(51, '');
    die "wrong length" unless length($qstr) == 51;
    for (split //, $qstr) {
        die "value too low" if ord()-33 < 20
    }
    
    $qstr = rand_qual(50, 'front');
    die "wrong length" unless length($qstr) == 50;
    $n = 0;
    for (split //, $qstr) {
        die "value too high" unless ord()-33 < 20;
        $n += 1;
        last if $n == 10;
    }
    
    $qstr = rand_qual(50, 'back');
    die "wrong length" unless length($qstr) == 50;
    $n = 0;
    for (split //, $qstr) {
        $n += 1;
        next unless $n > 40;
        die "value too high" unless ord()-33 < 20;
    }
    
    $qstr = rand_qual(50, 'front back');
    die "wrong length" unless length($qstr) == 50;
    $n = 0;
    for (split //, $qstr) {
        $n += 1;
        next if $n > 10 || $n <= 40;
        die "value too high" unless ord()-33 < 20;
    }
    
    $qstr = rand_qual(50, 'low');
    die "wrong length" unless length($qstr) == 50;
    $n = 0;
    for (split //, $qstr) {
        next unless ord() - 33 < 20;
        $n += 1;
    }
    die "too few low values" if $n + $n < length($qstr);
    
    $qstr = rand_qual(51, 'low');
    die "wrong length" unless length($qstr) == 51;
    $n = 0;
    for (split //, $qstr) {
        next unless ord() - 33 < 20;
        $n += 1;
    }
    die "too few low values" if $n + $n < length($qstr);
}
selftest();

$\ = "\n";
$, = "\t";
sub format_sam($$$$)
{
    print $_[0], 0015|($_[1] == 1 ? 0100 : 0200)|($_[2] ? 0 : 01000), '*', 0, 0, '*', '*', 0, 0, rand_read(length($_[3])), $_[3]
}

format_sam("goodgood75", 1, 1, rand_qual(75, ''));
format_sam("goodgood75", 2, 1, rand_qual(75, ''));
format_sam("goodgood50", 1, 1, rand_qual(50, ''));
format_sam("goodgood50", 2, 1, rand_qual(50, ''));

format_sam("goodbadlq75", 1, 1, rand_qual(75, 'low'));
format_sam("goodbadlq75", 2, 1, rand_qual(75, 'low'));
format_sam("goodbadlq50", 1, 1, rand_qual(50, 'low'));
format_sam("goodbadlq50", 2, 1, rand_qual(50, 'low'));

format_sam("goodbadf75", 1, 1, rand_qual(75, 'front'));
format_sam("goodbadf75", 2, 1, rand_qual(75, 'front'));
format_sam("goodbadf50", 1, 1, rand_qual(50, 'front'));
format_sam("goodbadf50", 2, 1, rand_qual(50, 'front'));

format_sam("goodbadb75", 1, 1, rand_qual(75, 'back'));
format_sam("goodbadb75", 2, 1, rand_qual(75, 'back'));
format_sam("goodbadb50", 1, 1, rand_qual(50, 'back'));
format_sam("goodbadb50", 2, 1, rand_qual(50, 'back'));

format_sam("badgood75", 1, 0, rand_qual(75, ''));
format_sam("badgood75", 2, 0, rand_qual(75, ''));
format_sam("badgood50", 1, 0, rand_qual(50, ''));
format_sam("badgood50", 2, 0, rand_qual(50, ''));

format_sam("badbadlq75", 1, 0, rand_qual(75, 'low'));
format_sam("badbadlq75", 2, 0, rand_qual(75, 'low'));
format_sam("badbadlq50", 1, 0, rand_qual(50, 'low'));
format_sam("badbadlq50", 2, 0, rand_qual(50, 'low'));

format_sam("badbadlq75", 1, 0, rand_qual(75, 'front'));
format_sam("badbadlq75", 2, 0, rand_qual(75, 'front'));
format_sam("badbadlq50", 1, 0, rand_qual(50, 'front'));
format_sam("badbadlq50", 2, 0, rand_qual(50, 'front'));

format_sam("badbadb75", 1, 0, rand_qual(75, 'back'));
format_sam("badbadb75", 2, 0, rand_qual(75, 'back'));
format_sam("badbadb50", 1, 0, rand_qual(50, 'back'));
format_sam("badbadb50", 2, 0, rand_qual(50, 'back'));
