#!/usr/bin/perl -w
use strict;
use warnings;

for (@ARGV) {
    if (/^-(?:h|\?|help)$/) {
        print "$0 [-h|-?|-help] [-shuffle] [-nD]"
    }
}

{
    local $\ = "\n";
    local $, = "\t";
    print '#GROUP', 'NAME', 'READNO', 'SEQUENCE', 'REFERENCE', '-STRAND', 'POSITION', 'CIGAR';
}

my $shuffle = !!0;
for (@ARGV) {
    if (/^-shuffle$/) {
        $shuffle = !0;
    }
}

if ($shuffle) {
    my $kid = open(KID, '-|') // die "can't fork: $!";
    if ($kid) {
        my @x;
        my $n = 0;
        while (defined(local $_ = <KID>)) {
            my $ii = int(rand($n + 1));
            ($_, $x[$ii]) = ($x[$ii], $_) if $ii < $n;
            push @x, $_;
            ++$n;
        }
        close KID;
        print @x;
        exit(0);
    }
}

my $n = 1;
my $st = 0;
for (@ARGV) {
    if ($st) {
        $n = 0+$1 if /^(\d+)$/;
        $st = 0;
        next;
    }
    if (/^-n(\d+)$/) {
        $n = 0+$1;
        next;
    }
    if (/^-n$/) {
        $st = 1;
        next;
    }
}

$\ = "\n";
$, = "\t";

my @NAMECHARS = split //, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
my $name;

sub randomName()
{
    $name = $NAMECHARS[int(rand($#NAMECHARS))];
    $name .= $NAMECHARS[int(rand($#NAMECHARS + 1))] for (1..20);
}

my $reference = '';
{
    my @B = qw{ A C G T };
    $reference .= $B[int(rand($#B + 1))] for (0..100000);
}

sub sequence
{
    my $s = substr($reference, $_[0], $_[1]);
    if ($_[2]) {
        $s =~ tr/ACGT/TGCA/;
        $s = reverse $s;
    }
    return $s;
}

print 'REJECTED', 'INVALID_CIGAR_1', 1, sequence(2345, 50), 'chr1', 'false', 2345, '60M';
print 'REJECTED', 'INVALID_CIGAR_1', 2, sequence(2345 + 450, 50, 1), 'chr1', 'true', 2345 + 450, '50M';

print 'REJECTED', 'INVALID_CIGAR_2', 1, sequence(2345, 50), 'chr1', 'false', 2345, '50M';
print 'REJECTED', 'INVALID_CIGAR_2', 2, sequence(2345 + 450, 50, 1), 'chr1', 'true', 2345 + 450, '50Z';

print 'REJECTED', 'INVALID_CIGAR_3', 1, sequence(2345, 50), 'chr1', 'false', 2345, '50D';
print 'REJECTED', 'INVALID_CIGAR_3', 2, sequence(2345 + 450, 50, 1), 'chr1', 'true', 2345 + 450, '50M';

print 'REJECTED', 'HALF_ALIGNED_1', 1, sequence(2345, 50), 'chr1', 'false', 2345, '50M';
print 'REJECTED', 'HALF_ALIGNED_1', 2, sequence(2345 + 450, 50, 1), '#chr1', 'true', 2345 + 450, '50M';

print 'REJECTED', 'HALF_ALIGNED_2', 1, sequence(3345 + 450, 50, 1), 'chr1', 'true', 3345 + 450, '50M';
print 'REJECTED', 'HALF_ALIGNED_2', 2, sequence(3345, 50), '#chr1', 'false', 3345, '50M';

print 'REJECTED', 'NOT_ALIGNED', 1, sequence(12345, 50), '#chr1', 'false', 12345, '50M';
print 'REJECTED', 'NOT_ALIGNED', 2, sequence(12345 + 450, 50, 1), '#chr1', 'true', 12345 + 450, '50M';

print 'REJECTED', 'MISMATCHED_SEQUENCE_1', 1, sequence(2345, 50), 'chr1', 'false', 2345, '50M';
print 'REJECTED', 'MISMATCHED_SEQUENCE_1', 1, sequence(12345, 50), 'chr1', 'false', 12345, '50M';
print 'REJECTED', 'MISMATCHED_SEQUENCE_1', 2, sequence(2345 + 450, 50, 1), 'chr1', 'true', 2345 + 450, '50M';

print 'REJECTED', 'MISMATCHED_SEQUENCE_2', 1, sequence(2355, 50), 'chr1', 'false', 2355, '50M';
print 'REJECTED', 'MISMATCHED_SEQUENCE_2', 2, sequence(2355 + 450, 50, 1), 'chr1', 'true', 2355 + 450, '50M';
print 'REJECTED', 'MISMATCHED_SEQUENCE_2', 2, sequence(12355 + 450, 50, 1), 'chr1', 'true', 12355 + 450, '50M';

while ($n > 0) {
    my @contig;
    for (1..50) {
        if (rand(2) < 1) {
            push @contig, { name => sprintf("SPOT_%02u", $_), readNo => 1, sequence => sequence(10000 + ($_ - 1) * 10, 50), strand => '+', position => 10000 + ($_ - 1) * 10, cigar => '50M' };
            push @contig, { name => sprintf("SPOT_%02u", $_), readNo => 2, sequence => sequence(10425 + ($_ - 1) * 10, 50), strand => '-' , position => 10425 + ($_ - 1) * 10, cigar => '50M' };
        }
        else {
            push @contig, { name => sprintf("SPOT_%02u", $_), readNo => 1, sequence => sequence(10425 + ($_ - 1) * 10, 50), strand => '-' , position => 10425 + ($_ - 1) * 10, cigar => '50M' };
            push @contig, { name => sprintf("SPOT_%02u", $_), readNo => 2, sequence => sequence(10000 + ($_ - 1) * 10, 50), strand => '+', position => 10000 + ($_ - 1) * 10, cigar => '50M' };
        }
    }
    for (sort { $a->{position} <=> $b->{position} } @contig) {
        print 'CONTIG_'.$n.'_chr1:10000-10965', @$_{'name', 'readNo', 'sequence'}, 'chr1', @$_{'strand', 'position', 'cigar'}
    }
    --$n;
}
