#!/usr/bin/perl -w
# =============================================================================
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
# =============================================================================
=pod
 
=head1 NAME
 
sam-parse-test-values - generate values to test SAM parsing

=head1 DESCRIPTION
 
 The script generates *SYNTACTICALLY VALID* test values for SAM.
 There is a small header with reference (SQ) records, followed by
 a set of special test values:

=over

=item an empty-but-valid record

=item QNAME of length > 1 that starts with '*'
 
=item QNAME consisting of the limits of the valid character set for QNAME
 
=item QNAME of maximum valid length and character set
 
=item FLAG of maximum value
 
=item RNAME of first reference in header

=item RNAME of last reference in header

=item POS of maximum value

=item MAPQ of maximum value

=item CIGAR with all operations and zero operation length
 
=item CIGAR with maximum operation length
 
=item RNEXT of first reference in header

=item RNEXT of last reference in header
 
=item RNAME and RNEXT that are different

=item RNAME and RNEXT that are same
 
=item RNAME and RNEXT of '='

=item PNEXT of maximum value
 
=item TLEN of most negative valid value
 
=item TLEN of most positive valid value
 
=item SEQ with all BAM-valid characters
 
=item SEQ with all BAM-valid characters lowercase
 
=item SEQ with all SAM-valid characters
 
=item QUAL of length > 1 that starts with '*'

=item QUAL with the entire valid character set of QUAL
 
=item EXTRA:A min/max values
 
=item EXTRA:i +-0, min/max values
 
=item EXTRA:f zeros and +/- zeros
 
=item EXTRA:f zeros with exponents, min/max exponents
 
=item EXTRA:f ones with exponents, min/max exponents

=item EXTRA:f various forms which should all equal 11.1

=item EXTRA:Z empty, valid char set, starts with ' ', ends with ' '
 
=item EXTRA:H empty, valid char set
 
=item EXTRA:B all subtypes with empty list
 
=item EXTRA:B signed integer subtypes: min/max values

=item EXTRA:B unsigned integer subtypes: min/max values
 
=item EXTRA:B:f repeat of EXTRA:f test values in list form
 
=back
 
 After the special test values, the script with print some number of
 test records with randomly filled in fields. About 2/3 of these will
 be mate pairs. None of these test records are quaranteed to be
 symantically correct. It is purposeful that very little effort is
 expended to try to make them symantically correct.
 
=head1 EXAMPLE

=head2 Print out special test values + 1000 random test values

 sam-parse-test-values

=head2 Print out special test values + 10 random test values

 sam-parse-test-values 10

=head2 Print out special test values + infinitely many random test values

 sam-parse-test-values 0

=cut

use warnings;
use strict;

my $RECORDS = shift // 1000;

my @ref = ();
{ # generate some random reference records
    my $n = int(rand(10)) + 5;
    my $i;
    for my $i (0 ... $n) {
        push @ref, { 'name' => "chr".($i+1), 'length' => int(rand(1_000_000)) + 20_000 };
    }
}

sub valid_QNAME_chars ()
{
    join '', map { chr } (ord('!') .. ord('?'), ord('A') .. ord('~'))
}

sub valid_SEQ_chars_strict ()
{
    "=ACMGRSVTWYHKDBN"
}

sub valid_SEQ_chars_loose ()
{
    join '', ('A' .. 'Z', 'a' .. 'z', '.', '=')
}

sub valid_QUAL_chars ()
{
    '@'.valid_QNAME_chars
}

sub tagChars ()
{
    join '', ('A' .. 'Z', 'a' .. 'z', '0' .. '9');
}

sub empty_SAM()
{
    {
        QNAME => '*',
        FLAG => 0,
        RNAME => '*',
        POS => 0,
        MAPQ => 0,
        CIGAR => '*',
        RNEXT => '*',
        PNEXT => 0,
        TLEN => 0,
        SEQ => '*',
        QUAL => '*',
        EXTRA => []
    }
}

sub random_character($;$);
sub scramble($);
sub random_string($;$);
sub random_SAM();
sub random_SEQ($;$);
sub random_QUAL($);
sub print_header($);
sub print_SAM($);

print_header(0);
print_SAM empty_SAM;
{ # QNAME edge case: starts with *
    my $rec = empty_SAM;
    $rec->{QNAME} = "*valid*";
    print_SAM $rec;
}
{ # QNAME edge case: limits of valid char set
    my $rec = empty_SAM;
    $rec->{QNAME} = scramble "!?A~";
    print_SAM $rec;
}
{ # QNAME edge case: max valid length limit
    my $rec = empty_SAM;
    $rec->{QNAME} = random_string valid_QNAME_chars, 254;
    print_SAM $rec;
}
{ # FLAG edge case: maximum value
    my $rec = empty_SAM;
    $rec->{FLAG} = 0xFFFF;
    print_SAM $rec;
}
{ # RNAME edge case: minimum value
    my $rec = empty_SAM;
    $rec->{RNAME} = $ref[0]->{name};
    print_SAM $rec;
}
{ # RNAME edge case: maximum value
    my $rec = empty_SAM;
    $rec->{RNAME} = $ref[-1]->{name};
    print_SAM $rec;
}
{ # POS edge case: maximum value
    my $rec = empty_SAM;
    $rec->{POS} = 0x7FFFFFFF;
    print_SAM $rec;
}
{ # MAPQ edge case: maximum value
    my $rec = empty_SAM;
    $rec->{MAPQ} = 0xFF;
    print_SAM $rec;
}
{ # CIGAR edge case: all opcodes, zero length
    my $rec = empty_SAM;
    $rec->{CIGAR} = "0H0S0M0I0D0N0=0X0S0H";
    print_SAM $rec;
}
{ # CIGAR edge case: maximum operation length
    my $rec = empty_SAM;
    $rec->{CIGAR} = 0xFFFFFFF . 'M';
    print_SAM $rec;
}
{ # RNEXT edge case: minimum value
    my $rec = empty_SAM;
    $rec->{RNEXT} = $ref[0]->{name};
    print_SAM $rec;
}
{ # RNEXT edge case: maximum value
    my $rec = empty_SAM;
    $rec->{RNEXT} = $ref[-1]->{name};
    print_SAM $rec;
}
{ # RNEXT edge case: special value
    my $rec = empty_SAM;
    $rec->{RNAME} = $ref[0]->{name};
    $rec->{RNEXT} = $ref[1]->{name};
    print_SAM $rec;
}
{ # RNEXT edge case: special value
    my $rec = empty_SAM;
    $rec->{RNEXT} = $rec->{RNAME} = $ref[0]->{name};
    print_SAM $rec;
}
{ # RNEXT edge case: special value
    my $rec = empty_SAM;
    $rec->{RNAME} = $ref[0]->{name};
    $rec->{RNEXT} = '=';
    print_SAM $rec;
}
{ # PNEXT edge case: maximum value
    my $rec = empty_SAM;
    $rec->{PNEXT} = 0x7FFFFFFF;
    print_SAM $rec;
}
{ # TLEN edge case: minimum value
    my $rec = empty_SAM;
    $rec->{TLEN} = -(0x7FFFFFFF+1);
    print_SAM $rec;
}
{ # TLEN edge case: maximum value
    my $rec = empty_SAM;
    $rec->{TLEN} = 0x7FFFFFFF;
    print_SAM $rec;
}
{ # SEQ edge case: strict char set
    my $rec = empty_SAM;
    $rec->{SEQ} = scramble valid_SEQ_chars_strict;
    print_SAM $rec;
}
{ # SEQ edge case: strict char set lower case
    my $rec = empty_SAM;
    $rec->{SEQ} = scramble lc valid_SEQ_chars_strict;
    print_SAM $rec;
}
{ # SEQ edge case: full char set
    my $rec = empty_SAM;
    $rec->{SEQ} = scramble valid_SEQ_chars_loose;
    print_SAM $rec;
}
{ # QUAL edge case: starts with *
    my $rec = empty_SAM;
    $rec->{QUAL} = '*' . random_QUAL(0);
    $rec->{SEQ} = random_SEQ(length $rec->{QUAL}); # length SEQ == length QUAL
    print_SAM $rec;
}
{ # QUAL edge case: full char set
    my $rec = empty_SAM;
    $rec->{QUAL} = random_QUAL(0);
    $rec->{SEQ} = random_SEQ(length $rec->{QUAL}); # length SEQ == length QUAL
    print_SAM $rec;
}
{ # EXTRA A edge cases
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:A:!";
    push $rec->{EXTRA}, "Z1:A:~";
    print_SAM $rec;
}
{ # EXTRA i cases
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:i:0";
    push $rec->{EXTRA}, "Z1:i:+0";
    push $rec->{EXTRA}, "Z2:i:-0";
    push $rec->{EXTRA}, sprintf("Z3:i:%i", (-0x7FFFFFFF)-1);
    push $rec->{EXTRA}, sprintf("Z4:i:%i", 0x7FFFFFFF);
    print_SAM $rec;
}
{ # EXTRA f cases: zeroes
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:f:0";
    push $rec->{EXTRA}, "Z1:f:0.0";
    push $rec->{EXTRA}, "Z2:f:0.0e0";
    push $rec->{EXTRA}, "Z3:f:+0";
    push $rec->{EXTRA}, "Z4:f:+0.0";
    push $rec->{EXTRA}, "Z5:f:+0.0e0";
    push $rec->{EXTRA}, "Z6:f:-0";
    push $rec->{EXTRA}, "Z7:f:-0.0";
    push $rec->{EXTRA}, "Z8:f:-0.0e0";
    print_SAM $rec;
}
{ # EXTRA f cases: zeroes with exponents
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:f:0e1";
    push $rec->{EXTRA}, "Z1:f:0e+1";
    push $rec->{EXTRA}, "Z2:f:0e-1";
    push $rec->{EXTRA}, "Z3:f:0e38";
    push $rec->{EXTRA}, "Z4:f:0e+38";
    push $rec->{EXTRA}, "Z5:f:0e-38";
    push $rec->{EXTRA}, "Z6:f:0e+0";
    push $rec->{EXTRA}, "Z7:f:0e-0";
    print_SAM $rec;
}
{ # EXTRA f cases: ones with exponents
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:f:1e1";
    push $rec->{EXTRA}, "Z1:f:1e+1";
    push $rec->{EXTRA}, "Z2:f:1e-1";
    push $rec->{EXTRA}, "Z3:f:1e38";
    push $rec->{EXTRA}, "Z4:f:1e+38";
    push $rec->{EXTRA}, "Z5:f:1e-38";
    push $rec->{EXTRA}, "Z6:f:1e+0";
    push $rec->{EXTRA}, "Z7:f:1e-0";
    print_SAM $rec;
}
{ # EXTRA f cases: 11.1
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:f:11.1";
    push $rec->{EXTRA}, "Z1:f:11.1e0";
    push $rec->{EXTRA}, "Z2:f:11.1e+0";
    push $rec->{EXTRA}, "Z3:f:11.1e-0";
    push $rec->{EXTRA}, "Z4:f:+11.1";
    push $rec->{EXTRA}, "Z5:f:+11.1e0";
    push $rec->{EXTRA}, "Z6:f:+11.1e+0";
    push $rec->{EXTRA}, "Z7:f:+11.1e-0";
    push $rec->{EXTRA}, "Z8:f:1.11e+1";
    push $rec->{EXTRA}, "Z9:f:111e-1";
    push $rec->{EXTRA}, "ZA:f:0.111e2";
    push $rec->{EXTRA}, "ZB:f:0.111e+2";
    push $rec->{EXTRA}, "ZC:f:+1.11e+1";
    push $rec->{EXTRA}, "ZD:f:+111e-1";
    push $rec->{EXTRA}, "ZE:f:+0.111e2";
    push $rec->{EXTRA}, "ZF:f:+0.111e+2";
    print_SAM $rec;
}
{ # EXTRA Z cases
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:Z:";
    push $rec->{EXTRA}, "Z1:Z:".random_string(' '.valid_QUAL_chars);
    push $rec->{EXTRA}, "Z2:Z: starts with a space";
    push $rec->{EXTRA}, "Z3:Z:ends with a space ";
    print_SAM $rec;
}
{ # EXTRA H cases
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:H:";
    push $rec->{EXTRA}, "Z1:H:".scramble("0123456789ABCDEF");
    print_SAM $rec;
}
{ # EXTRA B edge cases: empty
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:B:c";
    push $rec->{EXTRA}, "Z1:B:C";
    push $rec->{EXTRA}, "Z2:B:i";
    push $rec->{EXTRA}, "Z3:B:I";
    push $rec->{EXTRA}, "Z4:B:s";
    push $rec->{EXTRA}, "Z5:B:S";
    push $rec->{EXTRA}, "Z6:B:f";
    print_SAM $rec;
}
{ # EXTRA B edge cases: min/max signed integer values
    my $rec = empty_SAM;
    push $rec->{EXTRA}, sprintf("Z0:B:c,%i", (-0x7F)-1);
    push $rec->{EXTRA}, sprintf("Z1:B:c,%i", 0x7F);
    push $rec->{EXTRA}, sprintf("Z2:B:s,%i", (-0x7FFF)-1);
    push $rec->{EXTRA}, sprintf("Z3:B:s,%i", 0x7FFF);
    push $rec->{EXTRA}, sprintf("Z4:B:i,%i", (-0x7FFFFFFF)-1);
    push $rec->{EXTRA}, sprintf("Z5:B:i,%i", 0x7FFFFFFF);
    print_SAM $rec;
}
{ # EXTRA B edge cases: min/max unsigned integer values
    my $rec = empty_SAM;
    push $rec->{EXTRA}, sprintf("Z0:B:C,%i", 0);
    push $rec->{EXTRA}, sprintf("Z1:B:C,%i", 0xFF);
    push $rec->{EXTRA}, sprintf("Z2:B:S,%i", 0);
    push $rec->{EXTRA}, sprintf("Z3:B:S,%i", 0xFFFF);
    push $rec->{EXTRA}, sprintf("Z4:B:I,%i", 0);
    push $rec->{EXTRA}, sprintf("Z5:B:I,%i", 0xFFFFFFFF);
    print_SAM $rec;
}
{ # EXTRA B:f cases:
    my $rec = empty_SAM;
    push $rec->{EXTRA}, "Z0:B:f,0";
    push $rec->{EXTRA}, "Z1:B:f,0,0.0,0.0e0,+0,+0.0,+0.0e0,-0,-0.0,-0.0e0";
    push $rec->{EXTRA}, "Z2:B:f,0e1,0e+1,0e-1,0e38,0e+38,0e-38,0e+0,0e-0";
    push $rec->{EXTRA}, "Z3:B:f,1e1,1e+1,1e-1,1e38,1e+38,1e-38,1e+0,1e-0";
    push $rec->{EXTRA}, "Z4:B:f,11.1,11.1e0,11.1e+0,11.1e-0,+11.1,+11.1e0,+11.1e+0,+11.1e-0,1.11e+1,111e-1,0.111e2,0.111e+2,+1.11e+1,+111e-1,+0.111e2,+0.111e+2";
    print_SAM $rec;
}
if ($RECORDS > 0) {
    print_SAM random_SAM() for (1 .. $RECORDS)
}
else {
    print_SAM random_SAM() while (1)
}
exit 0;

sub print_header($) {
    local $, = "\t";
    local $\ = "\n";
    
    print '@HD', "VN:1.0";
    return if $_[0];
    for (@ref) {
        print '@SQ', 'SN:'.$_->{'name'}, 'LN:'.$_->{'length'};
    }
}

sub print_SAM($)
{
    local $, = "\t";
    local $\ = "\n";
    
    print
        $_[0]->{QNAME},
        $_[0]->{FLAG},
        $_[0]->{RNAME},
        $_[0]->{POS},
        $_[0]->{MAPQ},
        $_[0]->{CIGAR},
        $_[0]->{RNEXT} eq $_[0]->{RNAME} && ($_[0]->{RNAME} ne '*') ? '=' : $_[0]->{RNEXT},
        $_[0]->{PNEXT},
        $_[0]->{TLEN},
        $_[0]->{SEQ},
        $_[0]->{QUAL},
        @{$_[0]->{EXTRA}};
}

sub random_character($;$)
{
    my $s = shift;
    my $n = shift || 0;
    substr($s, int(rand($n > 0 ? $n : (length($s) + $n))), 1)
}

sub swap($$$)
{
    my $i = \substr $_[0], $_[1], 1;
    my $j = \substr $_[0], $_[2], 1;
    ( $$j, $$i ) = ( $$i, $$j )
}

sub scramble($)
{
    my $s = $_[0];
    my $N = length $s;
    swap $s, $_, int(rand($N - $_)) + $_ for (0 .. $N - 1);
    $s;
}

sub random_string($;$)
{
    my $gen = shift;
    my $len = shift || length($gen);
    my $s = '';
    do { $s .= $gen } while (length $s < $len);
    substr scramble $s, 0, $len
}

sub random_QNAME()
{
    random_string valid_QNAME_chars, int(rand(4)) + 4
}

sub random_Ref_Pos($$;$)
{
    my ($name, $pos);
    my $r;
    if (scalar(@_) == 3 && rand() < 0.5) {
        $r = $_[2];
    }
    $r = int(rand(scalar @ref)) unless defined $r;
    $name = $ref[$r]->{'name'};
    $pos = 1 + int(rand($ref[$r]->{'length'}));
    if ($_[1]) {
        $_[0]->{RNEXT} = $name;
        $_[0]->{PNEXT} = $pos;
        $_[0]->{RID_N} = $r;
    }
    else {
        $_[0]->{RNAME} = $name;
        $_[0]->{POS} = $pos;
        $_[0]->{RID} = $r;
    }
}

sub random_SEQ($;$)
{
    my $len = shift;
    my $gen = shift || "AAATTTCCGG";
    random_string $gen, $len
}

sub random_QUAL($)
{
    my $len = $_[0] == 0 ? length(valid_QUAL_chars) : $_[0];
    my $rslt = '';
    do { $rslt .= valid_QUAL_chars } while (length $rslt < $len);
    substr(scramble $rslt, 0, $len);
}

sub random_CIGAR($;$)
{
    my $seqlen = shift;
    my $maxlen = shift // $seqlen;
    my $s = '';
    my $h = $maxlen - $seqlen;
    my $len = $seqlen - int($seqlen * rand() * 0.25);

    if (rand() < 0.5) {
    MATCH:
        $s = $len .'M';
    }
    elsif (rand() < 0.5) {
        my $i = int($len * (rand() * 0.25 + 0.5));
        goto MATCH unless $i > 0;
        $s = sprintf("%iM%iD%iM", $i, int(rand(9)+1), $len - $i);
    }
    else {
        my $j = int(rand(9) + 1);
        goto MATCH unless $j < $len;
        my $i = int(($len - $j) * (rand() * 0.25 + 0.5));
        goto MATCH unless $i > 0;
        $s = sprintf("%iM%iI%iM", $i, $j, $len - ($i + $j));
    }
    $seqlen -= $len;
    if ($seqlen) {
        my $len = int(rand($seqlen) + 1);
        $s = $len . 'S' . $s;
        $seqlen -= $len;
        $s .= $seqlen . 'S' if $seqlen;
    }
    if ($h > 0) {
        my $len = int(rand($h) + 1);
        $s = $len . 'H' . $s;
        $h -= $len;
        $s .= $h . 'H' if $h;
    }
    return $s;
}

sub random_EXTRA_tag($)
{
    my $tag;
    do {
        $tag = random_character tagChars, -10; # digits not allowed in first character
        $tag .= random_character tagChars;
    } while exists $_[0]->{$tag};
    $_[0]->{$tag} = 1;
    $tag;
}

sub random_EXTRA_A()
{
    return sprintf "%s:A:%.1s", @_, random_character(valid_QUAL_chars);
}

sub random_u()
{
    my $s = '0' x int(rand(9) + 1);
    substr($s, $_, 1) = chr(int(rand(10)) + 48) for (0 .. length($s) - 1);
    my $i = 0;
    ++$i while ($i < length($s) - 2 && substr($s, $i, 1) eq '0');
    return substr($s, $i);
}

sub random_i()
{
    return (rand() < 0.5 ? '' : rand() < 0.5 ? '-' : '+').random_u();
}

sub random_u8()
{
    return ''.int(rand(256));
}

sub random_i8()
{
    return ''.((0+random_u8()) - 128);
}

sub random_u16()
{
    return ''.int(rand(65536));
}

sub random_i16()
{
    return ''.((0+random_u8()) - 32768);
}

sub random_f()
{
    my $f = random_i();
    my $d = '';
    my $e = '';
    if (rand() < 0.5) {
        $d = '.'.random_u();
    }
    if (rand() < 0.5) {
        $e = 'e';
        if (rand() < 0.5) {
            $e .= '-';
        }
        elsif (rand() < 0.5) {
            $e .= '+';
        }
        $e .= int(rand(38));
    }
    $f.$d.$e
}

sub random_EXTRA_i()
{
    sprintf "%.2s:i:%s", @_, random_i();
}

sub random_EXTRA_f()
{
    sprintf "%.2s:f:%s", @_, random_f();
}

sub random_EXTRA_Z()
{
    my $len = int(rand(200));
    sprintf "%.2s:Z:%s", @_, random_string(' '.valid_QUAL_chars, $len);
}

sub random_EXTRA_H()
{
    my $s = sprintf "%.2s:H:", @_;
    while (rand() < 0.5) {
        $s .= sprintf("%02X", int(rand(256)));
    }
    $s
}

sub random_EXTRA_B()
{
    my $t = int(rand(7));
    my $f = (\&random_i8, \&random_u8, \&random_i16, \&random_u16, \&random_i, \&random_u, \&random_f)[$t];
    my $s = sprintf("%.2s:B:%s", @_, substr("cCsSiIf", $t, 1));
    $s .= ','.$f->() while rand() < 0.5;
    $s
}

sub random_EXTRA($)
{
    ((\&random_EXTRA_A, \&random_EXTRA_i, \&random_EXTRA_f, \&random_EXTRA_Z, \&random_EXTRA_H, \&random_EXTRA_B)[int(rand(6))])->(random_EXTRA_tag($_[0]))
}

sub random_FLAG($)
{
    $_[0]->{FLAG} = int(rand(0x1000)) if rand() < 0.5; # keeps the reserved bits cleared
    
    # sometimes set some of the flag bits correctly
    $_[0]->{FLAG} |= 0x0004 if ($_[0]->{CIGAR} eq '*' || $_[0]->{RNAME} eq '*' || $_[0]->{POS} == 0);
    if (($_[0]->{FLAG} & 0x0001) == 0 && rand() < 0.5) {
        $_[0]->{FLAG} &= ~(0x0002 | 0x0008 | 0x0020 | 0x0040 | 0x0080);
    }
    
    # sometimes clear alignment based on flag bit
    if (($_[0]->{FLAG} & 0x0004) != 0 && rand() < 0.5) {
        $_[0]->{CIGAR} = '*';
        $_[0]->{RNAME} = '*';
        $_[0]->{MAPQ} = 0;
        $_[0]->{POS} = 0;
        $_[0]->{TLEN} = 0;
        $_[0]->{FLAG} &= ~(0x0002 | 0x0100 | 0x0800);
    }
    
    # sometimes clear alignment based on flag bit
    if (($_[0]->{FLAG} & 0x0008) != 0 && rand() < 0.5) {
        $_[0]->{RNEXT} = '*';
        $_[0]->{PNEXT} = 0;
        $_[0]->{TLEN} = 0;
    }
}

sub add_EXTRA($)
{
    my %tags;
    while (rand() < 0.5 && scalar(keys %tags) < (26+26)*(26+26+10)) {
        push $_[0]->{EXTRA}, random_EXTRA(\%tags);
    }
}

sub random_SAM_1()
{
    my $result = empty_SAM;
    $result->{QNAME} = random_QNAME if rand() < 0.5;
    random_Ref_Pos($result, 0) if rand() < 0.5;
    $result->{MAPQ} = int(rand(256)) if rand() < 0.5;
    random_Ref_Pos($result, 1, $result->{RID}) if rand() < 0.5;
    $result->{TLEN} = (int(rand(0xFFFFFFFF)) - 0x80000000) if rand() < 0.5;
    if (rand() < 0.5) {
        my $seqlen = int(rand(50)+25);
        $result->{SEQ} = random_SEQ($seqlen);
        $result->{QUAL} = random_QUAL($seqlen) if rand() < 0.5;
        $result->{CIGAR} = random_CIGAR($seqlen);
    }
    elsif (rand() < 0.5) {
        $result->{CIGAR} = random_CIGAR(int(rand(0xFFFFFF0) + 0x0F));
    }
    
    random_FLAG($result);
    add_EXTRA($result);
    return $result;
}

sub random_SAM_aligned_pair($$)
{
    my $seqlen = int(rand(50)+25);
    
    random_Ref_Pos($_[0], 0);
    random_Ref_Pos($_[0], 1, $_[0]->{RID});
    $_[1]->{RNAME} = $_[0]->{RNEXT};
    $_[1]->{POS}   = $_[0]->{PNEXT};
    $_[1]->{RNEXT} = $_[0]->{RNAME};
    $_[1]->{PNEXT} = $_[0]->{POS};
    if ($_[0]->{RNAME} eq $_[0]->{RNEXT}) {
        $_[1]->{TLEN} = -($_[0]->{TLEN} = int((rand() + 1.0) * ($_[0]->{PNEXT} - $_[0]->{POS})));
    }
    $_[0]->{SEQ} = random_SEQ($seqlen);
    $_[1]->{SEQ} = random_SEQ($seqlen);
    if (rand() < 0.5) {
        $_[0]->{QUAL} = random_QUAL($seqlen);
        $_[1]->{QUAL} = random_QUAL($seqlen);
    }
    $_[0]->{CIGAR} = random_CIGAR($seqlen);
    $_[1]->{CIGAR} = random_CIGAR($seqlen);
}

sub random_SAM_half_aligned_pair($$)
{
    my $seqlen = int(rand(50)+25);

    random_Ref_Pos($_[0], 0);
    $_[0]->{SEQ} = random_SEQ($seqlen);
    $_[0]->{QUAL} = random_QUAL($seqlen) if rand() < 0.5;
    $_[0]->{CIGAR} = random_CIGAR($seqlen);
    if (rand() < 0.5) {
        $_[1]->{SEQ} = random_SEQ($seqlen);
        $_[1]->{QUAL} = random_QUAL($seqlen) if rand() < 0.5;
    }
}

my $mate;

sub random_SAM_pair()
{
    my $result;
    
    $result = empty_SAM;
    $mate = empty_SAM;
    
    $mate->{QNAME} = $result->{QNAME} = random_QNAME;
    
    if (rand() < 0.5) {
        if (rand() < 0.5) { #both aligned
            random_SAM_aligned_pair $result, $mate;
        }
        # else both unaligned
    }
    else { # one aligned, one not
        if (rand() < 0.5) {
            random_SAM_half_aligned_pair $result, $mate;
        }
        else {
            random_SAM_half_aligned_pair $mate, $result;
        }
    }
    random_FLAG($mate);
    add_EXTRA($mate);
    random_FLAG($result);
    $result->{FLAG} = ($result->{FLAG} & ~0x00C0) | (($mate->{FLAG} & 0x00C0) ^ 0x00C0);
    add_EXTRA($result);
    return $result;
}

sub random_SAM()
{
    my $result = $mate;

    goto (rand() < 0.5 ? \&random_SAM_1 : \&random_SAM_pair) unless defined $result;

    $mate = undef;
    return $result;
}
