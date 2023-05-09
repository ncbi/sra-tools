#!perl -w

use strict;
use warnings;
use Data::Dumper;

my $shuffle = 1;
my $align = 0.75;
my $count = 100;
my @readLayout = ();

sub someLayout() {
    $readLayout[rand(scalar @readLayout)]
}

sub someRead($) {
    join('', map { substr("ACGT", rand(4), 1) } 1..$_[0])
}

sub max ($$) { $_[$_[0] < $_[1]] }
sub min ($$) { $_[$_[0] > $_[1]] }

sub readType($$) {
    my @type = ();
    if ($_[0] eq 'T') {
        push @type, 'SRA_READ_TYPE_TECHNICAL';
    }
    else {
        push @type, 'SRA_READ_TYPE_BIOLOGICAL';
        push @type, 'SRA_READ_TYPE_FORWARD' if $_[1] eq 'F';
        push @type, 'SRA_READ_TYPE_REVERSE' if $_[1] eq 'R';
    }
    join('|', @type)
}

sub parseLayout($) {
    my $layout = $_[0];
    local $_;
    my ($min, $max, $len, $bt, $fr) = (1, 150, undef, '', '');
    for (split /\s*,\s*/, $layout) {
        my @reads = ();
        while ($_) {
            my $l = length;
            if (/^(\d+)(-(\d*)){0,1}(.*)/) {
                if ($2) {
                    ($len, $min) = (undef, 0+$1);
                    $max = 0+$3 if defined($3);
                }
                else {
                    ($len, $min, $max) = (0+$1, 1, 150);
                }
                $_ = $4;
            }
            ($bt, $_) = ($1, $2) if (/^([BT])(.*)/);
            ($fr, $_) = ($1, $2) if (/^([FR])(.*)/);
            
            if ($l == length) {
                die "can't parse layout: $layout\n";
            }
            push @reads
                , { 'min-length' => $len ? $len : defined($min) ? $min : 1
                  , 'max-length' => $len ? $len : defined($max) ? $max : 150
                  , 'type' => defined($bt) || defined($fr) ? readType($bt, $fr) : undef
                }
        }
        push @readLayout, \@reads;
    }
}

for (@ARGV) {
    if (/^no-shuffle$/) {
        $shuffle = 0;
        next;
    }
    if (/^align(?:=(\d+))?$/i) {
        $align = defined($1) ? (0+$1 / 100.0) : 1.0;
        next;
    }
    if (/^layout=(.+)$/) {
        parseLayout($1);
        next;
    }
    if (/help/) {
        print "$0 [no-shuffle] [align(=<pct>)] [layout=<layout>] [help] [<count:=100>]\n";
        print <<'HELP';
  layout is (\d+)(-(\d+)?)([BT])([FR])
    multiple reads are concatenated.
    multiple layouts are comma separated.
    multiple layouts are used randomly.
    Examples:
      150BF75TR :
        1 forward biological read of length 150,
        1 reverse technical read of length 75
      150BFR :
        2 biological reads of length 150,
        one forward and one reverse, (150B is reused)
      150BFR,RF : (these are the defaults)
        two layouts, 2 biological reads of length 150,
        one forward and one reverse, and then one reverse and one forward
HELP
        exit 0;
    }
    if (/^(\d+)$/) {
        $count = 0+$1;
        next;
    }
    print "usage: $0 [no-shuffle] [align(=<pct>)] [layout=<layout>] [help] [<count>]\n";
    exit 1;
}

parseLayout("150BFR,RF") unless @readLayout;

my %stats;
$stats{'reads/biological'} = 0;
$stats{'reads/technical'} = 0;

sub recordSequence($$) {
    local $_;
    for (split '', $_[0]) {
        my $b = $_;
        $b =~ tr/ACGT/WSSW/;
        my $k = join('/', 'bases', $b, $_[1]);
        $stats{$k} = ($stats{$k} // 0) + 1
    }
    $stats{'reads/'.$_[1]} += 1;
}

my $nextAlignID = 1;
sub makeRecords() {
    my @result = ();
    my $layout = someLayout;
    my $readSeq = '';
    my @readType = ();
    my @readLen = ();
    my @readStart = ();
    my @alignID = ();
    my $spotGroup = "MOOF";
    
    for (@$layout) {
        my $minlen = min($_->{'min-length'}, $_->{'max-length'});
        my $range = max($_->{'min-length'}, $_->{'max-length'}) - $minlen;
        my $len = ($range ? int(rand($range+1)) : 0) + $minlen;
        my $seq = someRead($len);
        my $aligned = 0;
        my $bio = !!($_->{'type'} =~ 'SRA_READ_TYPE_BIOLOGICAL');

        if ($bio) {
            recordSequence($seq, 'biological');
        }
        else {
            recordSequence($seq, 'technical');
        }
        
        push @readStart, ((@readStart && @readLen) ? ($readStart[-1] + $readLen[-1]) : 0);
        push @readLen, $len;
        # push @readStart, @readStart ? ($readStart[-1] + $readLen[-1]) : 0;
        push @readType, $_->{'type'};
        if ($align > 0 && $bio) {
            $aligned = $align >= 1 ? 1 : (rand() < $align);
        }
        if ($aligned) {
            my $refpos = int(rand(300_000 - 10_000) + 10_000);
            my $cigar = "".$len."M";
            my $strand = rand() < 0.5 ? '0' : '1';
            my $refname = "COWDOG_22";
            
            $stats{'alignments'} = ($stats{'alignments'} // 0) + 1;
            push @result, join("\t", $seq, $refname, $refpos, $strand, $cigar, $spotGroup);
            push @alignID, $nextAlignID++;
        }
        else {
            $readSeq .= $seq;
            push @alignID, 0;
        }
    }
    $stats{'spots/'.(scalar @$layout).'/total'} = ($stats{'spots/'.(scalar @$layout).'/total'} // 0) + 1;
    push @result, join("\t", $readSeq, join(',', @readLen), join(',', @readStart), join(',', @readType), join(',', @alignID), $spotGroup);
    return @result;
}

my $n = 0;
while ($n < $count) {
    my @records = makeRecords;
    {
        local $, = "\n"; 
        local $\ = "\n";
        print @records;
    }
    $n += scalar @records;
}
# print STDERR Dumper(\%stats);

open STATS, ">summary.json";
print STATS "{\n";
print STATS "\t", '"total": {', "\n";
print STATS "\t" x 2, '"reads": {', "\n";
print STATS "\t" x 3, '"count": ', ($stats{'reads/biological'} // 0) + ($stats{'reads/technical'} // 0), ",\n";
print STATS "\t" x 3, '"biological": ', ($stats{'reads/biological'} // 0), ",\n";
print STATS "\t" x 3, '"technical": ', ($stats{'reads/technical'} // 0), "\n";
print STATS "\t" x 2, '}', ",\n";
print STATS "\t" x 2, '"bases": [', "\n";
print STATS "\t" x 3, '{', "\n";
print STATS "\t" x 4, '"base": "S",', "\n"; 
print STATS "\t" x 4, '"biological": ', $stats{"bases/S/biological"} // 0, ",\n";
print STATS "\t" x 4, '"technical": ', $stats{"bases/S/technical"} // 0, "\n";
print STATS "\t" x 3, '}', ",\n"; 
print STATS "\t" x 3, '{', "\n";
print STATS "\t" x 4, '"base": "W",', "\n"; 
print STATS "\t" x 4, '"biological": ', $stats{"bases/W/biological"} // 0, ",\n";
print STATS "\t" x 4, '"technical": ', $stats{"bases/W/technical"} // 0, "\n";
print STATS "\t" x 3, '}', "\n"; 
print STATS "\t" x 2, ']', ",\n";
print STATS "\t" x 2, '"spots": [', "\n";

my @statlayouts = grep { /^spots/ } keys %stats;
for my $i (0..$#statlayouts) {
    local $_ = $statlayouts[$i];
    next unless m{^spots/(\d+)/total$};

    my $comma = ($i == $#statlayouts) ? '' : ',';

    print STATS "\t" x 3, '{', "\n";
    print STATS "\t" x 4, '"nreads": ', $1, ",\n"; 
    print STATS "\t" x 4, '"total": ', $stats{$_}, "\n"; 
    print STATS "\t" x 3, '}', $comma, "\n";
}
print STATS "\t" x 2, ']', ",\n";
print STATS "\t" x 2, '"references": [', "\n";
print STATS "\t" x 3, '{', "\n";
print STATS "\t" x 4, '"name": "COWDOG_22"', ",\n"; 
print STATS "\t" x 4, '"alignments": {', "\n";
print STATS "\t" x 5, '"total": ', $stats{'alignments'} // 0, "\n";
print STATS "\t" x 4, '}', "\n"; 
print STATS "\t" x 3, '}', "\n";
print STATS "\t" x 2, ']', "\n";
print STATS "\t", '}', "\n";
print STATS "}\n";
