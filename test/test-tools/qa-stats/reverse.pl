#!perl -w

use strict;
use warnings;

sub splitAtTab($) {
    my $tab = index $_[0], "\t";
    return (\$_[0], \substr($_[0], length($_[0]), 0)) if ($tab < 0);
    return (\substr($_[0], 0, $tab), \substr($_[0], $tab + 1));
}

sub numerics($) {
    local $_;
    my @result;
    for (split /\s*,\s*/, $_[0]) {
        return undef unless /(\d+)/;
        push @result, 0+$1;
    }
    return \@result;
}

LINE: while (defined($_ = <>)) {
    next if /^#/;

    my ($seq, $rl, $rs, $algn);
    my $n = 0;
    for my $fld (\split /\t/, $_) {
        ++$n;
        if ($n == 1) {
            $seq = $fld;
            next;
        }
        my $num = numerics($$fld);
        if ($n == 2) {
            next LINE unless defined $num;
            $rl = $num;
            next;
        }
        next unless defined $num && scalar(@$rl) == scalar(@$num);
        if ($n == 3) {
            $rs = $num;
            next;
        }
        $algn = $num;
        last;
    }
    next unless $n > 0;

    if ($n > 1 && scalar(@$rl) > 1) {
        if (!defined($rs)) {
            my @a;
            my $o = 0;
            for my $l (@$rl) {
                push @a, $o;
                $o += $l;
            }
            $rs = \@a;
        }
        if (defined($algn) && length($$seq) < $rs->[-1] + $rl->[-1]) {
            # adjust for omitted aligned bases
            my $rm = 0;
            for my $i (0..(scalar(@$rl)-1)) {
                if ($algn->[$i] != 0) {
                    $rs->[$i] -= $rm; # read start is moved back by the number of omitted bases
                    $rm += $rl->[$i]; # number of omitted bases is increased by the read length
                    $rl->[$i] = 0; # the read length becomes 0
                }
            }
        }
        for my $i (0..(scalar(@$rl)-1)) {
            my $s = \substr($$seq, $rs->[$i], $rl->[$i]);
            $$s =~ tr/ACGT/TGCA/;
            $$s = reverse $$s;
        }
        next;
    }
    $$seq =~ tr/ACGT/TGCA/;
    $$seq = reverse $$seq;
}
continue {
    print
}
