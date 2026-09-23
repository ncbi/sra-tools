#!perl -w

use strict;
use warnings;
use v5.26;
use feature "switch";
use experimental qw( switch );
use IO::File;

my $HD = join "\t", qw{ @HD VN:1.0 SO:unsorted };
my %SQ = ();
my %RG = ();
my %SAM = ();

sub parse_header_line($$$)
{
    my ($line, $tag, $kind) = @_;
    my @t = split /\t/, $line;
    my %r = map { /(..):(.+)/; { $1 => $2 } } @t[1..$#t];
    my $name = $r{$tag};
    die "expected $tag tag in $kind line at $.:\n$line\n" unless $name;
    return { name => $name, count => 0, line => $line }
}

while (defined(local $_ = <>)) {
    chomp;
    
    my $data = scalar %SAM;

SAM:
    # handle regular SAM lines
    if ($data) {
        my @F = \split /\t/;
        die "truncated SAM at line $.:\n$_\n" if $#F < 10;
        my $QNAME = ''.${$F[ 0]};
        
        for (@F[11..$#F]) {
            if ($$_ =~ /RG:Z:(.+)/) {
                $QNAME = "$1\t$QNAME";
                last
            }
        }
        unless ($SAM{$QNAME}) {
            $SAM{$QNAME} = { name => $QNAME, SAM => [], firstLine => $. }
        }
        push @{$SAM{$QNAME}->{'SAM'}}, $_;
        next;
    }
    unless (/^[@]/) {
        # not a header line
        $data = 1;
        goto SAM;
    }
    
    # handle SAM header lines
    my $line = $_;
    die "unexpected header line at $.:\n$_\n" unless /^[@](HD|SQ|RG|PG|CO)/;
    given ($1) {
#       when (/HD/) { do not want }
        when (/SQ/) {
            my $r = parse_header_line $line, 'SN', 'SQ';
            $SQ{$r->{name}} = $r;
        }
        when (/RG/) {
            my $r = parse_header_line $line, 'ID', 'RG';
            $RG{$r->{name}} = $r;
        }
    }
}
printf STDERR "Read %u records for %u spots.\n", $., scalar(keys %SAM);

for (keys %SAM) {
    my $rec = $SAM{$_};
    my $aligned = 0;
    my $filter = 0;
    for (@{$rec->{'SAM'}}) {
        my @F = \split /\t/;
        my $FLAG = 0+${$F[1]};
        ++$aligned if ($FLAG & 0x004) == 0;
        $filter |= ($FLAG & 0x200) != 0;
        $filter |= ($FLAG & 0x400) != 0;
    }
    $rec->{'aligned'} = $aligned;
    delete $SAM{$_} if ($filter || $aligned != 2);
}

sub make_unaligned($$$)
{
    my $rec = $SAM{$_[0]};
    return if $rec->{'aligned'} == 0;
    my $self = \$rec->{'SAM'}->[$_[1]];
    my $mate = \$rec->{'SAM'}->[$_[2]];
    my @SELF = split /\t/, ${$self};
    return if ((0+$SELF[1]) & 0x004) != 0; # already unmapped;
    
    my @MATE = split /\t/, ${$mate};

    $SELF[1] = (0+$SELF[1]) | 0x004;    # FLAG; self is unmapped.
    $MATE[1] = (0+$MATE[1]) | 0x008;    # mate FLAG; mate is unmapped.

    if (($SELF[1] & 0x010) != 0) {
        $SELF[9] =~ tr/ACGT/TGCA/;
        $SELF[9] = scalar reverse $SELF[9];
        $SELF[10] = scalar reverse $SELF[10];
    }
    $SELF[6] = $MATE[2];        # fix RNEXT
    $SELF[7] = $MATE[3];        # fix PNEXT
    $SELF[2] = $MATE[6] = '*';  # RNAME and mate RNEXT
    $SELF[3] = $MATE[7] = 0;    # POS and mate PNEXT
    $MATE[8] = 0;               # TLEN
    $SELF[4] = 0;               # MAPQ
    $SELF[5] = '*';             # CIGAR
    
    ${$self} = join "\t", @SELF;
    ${$mate} = join "\t", @MATE;
    
    --$rec->{'aligned'};
}

### Make about 50% of the spots have some unaligned reads.
for (keys %SAM) {
    next if rand() < 0.5;
    make_unaligned($_, 0, 1);
    next if rand() < 0.5;
    make_unaligned($_, 1, 0);
}

### Keep 2000 spots.
{
    my %keep = ();
    $keep{$_} = $SAM{$_} for (keys %SAM)[0..1999];
    %SAM = %keep;
}

### Get the usage count on header read groups and reference sequences.
for (keys %SAM) {
    for (@{$SAM{$_}->{'SAM'}}) {
        my @F = \split /\t/;
        my $FLAG = 0+${$F[1]};
        
        if (($FLAG & 0x004) == 0 && ''.${$F[2]} ne '*') {
            ++$SQ{''.${$F[2]}}->{'count'};
        }
        if (($FLAG & 0x008) == 0 && ''.${$F[6]} ne '*' && ''.${$F[6]} ne '=') {
            ++$SQ{''.${$F[6]}}->{'count'};
        }
        for (@F[11..$#F]) {
            if ($$_ =~ /RG:Z:(.+)/) {
                ++$RG{$1}->{'count'};
                last;
            }
        }
    }
}

### Only keep the read groups that are used.
{
    my %keep = ();
    $keep{$_} = $RG{$_} for grep { $RG{$_}->{'count'} } keys %RG;
    %RG = %keep;
}

### Only keep the reference sequences that are used.
{
    my %keep = ();
    $keep{$_} = $SQ{$_} for grep { $SQ{$_}->{'count'} } keys %SQ;
    %SQ = %keep;
}

printf STDERR "Processing %u spots.\n", scalar(keys %SAM);

### Count the bases, total and non-ACGT.
my @N = (0, 0, 0); ###< non-ACGT
my @n = (0, 0, 0); ###< total base count per file; [0] is combined

for (keys %SAM) {
    my $file = int(rand(2) + 1);
    my $rn = 0;
    my $rN = 0;
    for (@{$SAM{$_}->{'SAM'}}) {
        my @F = \split /\t/;
        for (split '', ''.${$F[9]}) {
            ++$rn;
            ++$rN if !/[ACGT]/;
        }
    }
    $SAM{$_}->{'n'} = $rn;
    $SAM{$_}->{'N'} = $rN;
    $SAM{$_}->{'file'} = $file;
    $n[$file] += $rn;
    $N[$file] += $rN;
}
$n[0] = $n[1] + $n[2];
$N[0] = $N[1] + $N[2];

my $first = 1; $first = 2 if $n[1] < $n[2];
my $last = 3 - $first;

printf "Total number of bases in file 1: %u\n", $n[1];
printf "Total number of bases in file 2: %u\n", $n[2];
printf "Total number of bases in both files: %u\n", $n[0];

printf "Total number of non-ACGT bases in file 1: %u (%.2f%%)\n", $N[1], (100.0 * $N[1]) / (1.0 * $n[1]);
printf "Total number of non-ACGT bases in file 2: %u (%.2f%%)\n", $N[2], (100.0 * $N[2]) / (1.0 * $n[2]);
printf "Total number of non-ACGT bases in both files: %u (%.2f%%)\n", $N[0], (100.0 * $N[0]) / (1.0 * $n[0]);

printf "Adding non-ACGT bases to both files, limit is %u in file %u.\n", ($n[$last] >> 1) - 1, $last;

### One file must have fewer than 50% non-ACGT
while (2 * ($N[$last] + 1) < $n[$last]) {
    my $key = (keys %SAM)[rand keys %SAM];
    my $spot = $SAM{$key};
    next if $spot->{'N'} * 2 + 1 >= $spot->{'n'};
    
    my $read = int(rand(2));
    my $file = $spot->{'file'};
    my @F = split /\t/, $spot->{'SAM'}->[$read];
    my $seq = ''.$F[9];
    my $acgt = ($seq =~ tr/ACGT//);
    
    next unless length $seq < 2 * $acgt + 1;
    
    while (1) {
        my $b = int(rand length $seq);
        next if substr($seq, $b, 1) !~ /[ACGT]/;
        substr($seq, $b, 1) = 'N';
        last;
    }
    $F[9] = $seq;
    $spot->{'SAM'}->[$read] = join "\t", @F;
    ++$spot->{'N'};
    ++$N[$file];
    ++$N[0];
}

printf "Total number of non-ACGT bases in file 1: %u (%.2f%%)\n", $N[1], (100.0 * $N[1]) / (1.0 * $n[1]);
printf "Total number of non-ACGT bases in file 2: %u (%.2f%%)\n", $N[2], (100.0 * $N[2]) / (1.0 * $n[2]);
printf "Total number of non-ACGT bases in both files: %u (%.2f%%)\n", $N[0], (100.0 * $N[0]) / (1.0 * $n[0]);

printf "Need to add about %u more non-ACGT bases to file %u.\n", ($n[0] >> 1) - $N[0], $first;

### The two files together must have more than 50% non-ACGT
while (2 * $N[0] <= $n[0]) {
    my $key = (keys %SAM)[rand keys %SAM];
    my $spot = $SAM{$key};
    next unless $spot->{'file'} == $first;
    
    my $read = int(rand(2));
    my @F = split /\t/, $spot->{'SAM'}->[$read];
    my $seq = ''.$F[9];
    
    while (1) {
        my $b = int(rand length $seq);
        next if substr($seq, $b, 1) !~ /[ACGT]/;
        substr($seq, $b, 1) = 'N';
        last;
    }
    $F[9] = $seq;
    $spot->{'SAM'}->[$read] = join "\t", @F;
    ++$spot->{'N'};
    ++$N[$first];
    ++$N[0];
}

printf "Total number of non-ACGT bases in file 1: %u (50%% %+i)\n", $N[1], $N[1] - ($n[1] >> 1);
printf "Total number of non-ACGT bases in file 2: %u (50%% %+i)\n", $N[2], $N[2] - ($n[2] >> 1);
printf "Total number of non-ACGT bases in both files: %u (50%% %+i)\n", $N[0], $N[0] - ($n[0] >> 1);

my @fh = (undef, undef, undef);

$fh[0] = IO::File->new("VDB-6398.header.sam", 'w') or die "can't open output header file: $!";
$fh[$first] = IO::File->new("VDB-6398_fail.sam", 'w') or die "can't open output file $first: $!";
$fh[$last]  = IO::File->new("VDB-6398_pass.sam", 'w') or die "can't open output file $last: $!";

IO::Handle->output_record_separator("\n");
IO::Handle->output_field_separator("\n");

# write the common SAM header.
$fh[0]->print($HD);
$fh[0]->print(map {$_->{'line'}} values %SQ);
$fh[0]->print(map {$_->{'line'}} values %RG);
$fh[0]->close();

#write the two SAM files.
$fh[$_->{'file'}]->print(@{$_->{'SAM'}}) for values %SAM;

$fh[1]->close();
$fh[2]->close();
