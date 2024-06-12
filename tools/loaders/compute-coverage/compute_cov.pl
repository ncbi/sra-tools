#!/usr/bin/perl -w

# ===========================================================================
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

use strict;

`sra-pileup -h 2>&1`;
die "add sra-pileup to PATH to run $0" if ( $? );

my $last_ref = "";

if ( $#ARGV != 0 ) {
    die "Usage: $0 <accession>\n";
}
my $run = $ARGV[0];
$run = "SRR543323" unless ( $run );

open IN, "sra-pileup $run -n |"  or die "unable to open $run";

my ( $q0, $q1, $q2, $q3, $q4, $NUM, @RC );
while(<IN>) {
    chop;
    my @in = split;
    my $ref = $in[0];
    my $cov = $in[3];
    if ( $ref ne $last_ref ) {
        $q0 = $q1 = $q2 = $q3 = $q4 = -1;
        for ( my $x = 1, my $c = 0; $x <= $#RC; $x++ ) {
            $c += $RC[$x];
            if ( $q0 == -1 && $c > ($NUM/10) ) {
                $q0 = $x;
            }
            if ( $q1 == -1 && $c > ($NUM/4) ) {
                $q1 = $x;
            }
            if ( $q2 == -1 && $c > ($NUM/2) ) {
                $q2=$x;
            }
            if ( $q3 == -1 && $c > ($NUM*3/4) ) {
                $q3=$x;
            }
            if ( $q4 == -1 && $c > ($NUM*0.9) ) {
                $q4=$x;
            }
            
            $RC[$x]=0;
        }
        if ( $last_ref ) {
            printf("$last_ref\t%d\t%d\t%d\t%d\t%d\n",$q0,$q1,$q2,$q3,$q4);
        }
        $last_ref = $ref;
        $NUM = 0;
    }
    $RC[$cov]++;
    $NUM++;
}
close IN;
printf("$last_ref\t%d\t%d\t%d\t%d\t%d\n",$q0,$q1,$q2,$q3,$q4);
