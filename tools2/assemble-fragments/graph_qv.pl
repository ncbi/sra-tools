#!/usr/bin/perl -w

=head1 NAME
 
 graph_qv

=head1 DESCRIPTION

This script graphs the distribution of quality values in a SRA2 fragments
object. Uses gnuplot and vdb-dump.
 
=head1 USAGE

graph_qv.pl [-terminal=<string>] [-output=<string>] [<SRA2 fragments object>]

=head1 EXAMPLE

graph_qv.pl SRR5054227.fragments
or
vdb-dump SRR5054227.fragments -T QUAL -f tab -C "READNO,READ_CYCLE,QUALITY,COUNT" | graph_qv.pl
 
=cut

use strict;
use warnings;
use IO::Handle;

sub which($) {
    for my $path (split ":", $ENV{PATH}) {
        my $full = "$path/$_[0]";
        return $full if -x $full;
    }
    die "no $_[0] in $ENV{PATH}"
}

my $gnuplot = which 'gnuplot';
my (@R1, @R2);
my $gnuplotTerminal = undef;
my $gnuplotOutput = "Read_";
my @runs;

if (@ARGV) {
    for (0..(scalar(@ARGV)-1)) {
        local $_ = $ARGV[$_];
        if (/^-terminal=(.+)/) {
            $gnuplotTerminal = $1;
            next
        }
        if (/^-output=(.+)/) {
            $gnuplotOutput = $1;
            next
        }
        push @runs, $_;
    }
}

sub report($) {
    my $r = $_[0];
    my $output = $gnuplotOutput."$r";

    my $graphCmd = <<"GNUPLOT";
set output "$output";
set autoscale;
set xlabel "Cycle";
set ylabel "Quality (phred)";
set title "Distribution of Quality Scores: Read $r";
set key bottom left;
plot '-' title "Median (+/- one quartile)" with errorlines,
     '-' title "Mode" with lines linewidth 2;
GNUPLOT
    $graphCmd = "set terminal $gnuplotTerminal;\n$graphCmd" if defined $gnuplotTerminal;
    
    my $kid = open CHILD, '|-' // die "can't fork: $!";
    if ($kid == 0) {
        exec {$gnuplot} $gnuplot, '-e', $graphCmd;
        die "can't exec $gnuplot: $!";
    }

    my $R = $r == 1 ? \@R1 : \@R2;
    my $N = scalar(@$R);
    my $medianGraph = sub {
        for my $c (0..$N-1) {
            my $M = scalar(@{$R->[$c]});
            my $S = 0.0;
            for my $q (0..$M-1) {
                next unless $R->[$c]->[$q];
                my $n = $R->[$c]->[$q];
                $S += $n;
            }
            my $mp = $S / 2.0;
            my $qp1 = $mp / 2.0;
            my $qp2 = $mp + $qp1;
            my $accum = 0;
            my ($median, $q1, $q2);
            for my $q (0..$M-1) {
                next unless $R->[$c]->[$q];
                my $n = $R->[$c]->[$q];
                my $nccum = $accum + $n;
                $median = $q if $mp >= $accum && $nccum > $mp;
                $q1 = $q if $qp1 >= $accum && $nccum > $qp1;
                $q2 = $q if $qp2 >= $accum && $nccum > $qp2;
                $accum = $nccum;
            }
            printf CHILD ("%i\t%i\t%i\t%i\n", $c+1, $median, $q1, $q2);
        }
        printf CHILD ("e\n");
    };
    my $modeGraph = sub {
        for my $c (0..$N-1) {
            my $M = scalar(@{$R->[$c]});
            my $S = 0.0;
            my ($max, $mode) = (0, 0);
            for my $q (0..$M-1) {
                next unless $R->[$c]->[$q];
                my $n = $R->[$c]->[$q];
                ($max, $mode) = ($n, $q) if $max < $n;
            }
            printf CHILD ("%i\t%i\n", $c+1, $mode);
        }
        printf CHILD ("e\n");
    };
    $medianGraph->();
    $modeGraph->();
}

sub parse {
    my @F = \split "\t";
    return (0+${$F[0]}, 0+${$F[1]}, ord(${$F[2]})-33, 0+${$F[3]})
}

sub load_data($) {
    my $fh = shift;
    while (defined(local $_ = <$fh>)) {
        my ($r, $c, $q, $n) = parse;
        my $R = $r == 1 ? \@R1 : \@R2;
        my $a = $R->[$c] || [];
        $a->[$q] = ($a->[$q] || 0) + $n;
        $R->[$c] = $a;
    }
    return $fh->input_line_number;
}

if (@runs) {
    for my $run (@runs) {
        my $vdbdump = which 'vdb-dump';
        my $kid = open CHILD, '-|' // die "can't fork: $!";
        if ($kid == 0) {
            exec {$vdbdump} $vdbdump, $run, qw{ -T QUALITY_STATISTICS -f tab -C }, 'READNO,READ_CYCLE,QUALITY,COUNT';
            die "can't exec $vdbdump: $!";
        }
        print STDERR "prog: reading from $run.QUALITY_STATISTICS\n";
        my $lines = load_data(IO::Handle->new_from_fd(fileno(CHILD), 'r'));
        print STDERR "info: loaded $lines records from $run.QUALITY_STATISTICS\n";
    }
}
else {
    print STDERR "prog: reading from stdin\n";
    my $lines = load_data(IO::Handle->new_from_fd(fileno(STDIN), 'r'));
    print STDERR "info: loaded $lines records\n";
}

print STDERR "prog: generating graphs\n";
report 1;
report 2;
