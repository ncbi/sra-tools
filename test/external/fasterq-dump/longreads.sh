#! /bin/sh
# ==============================================================================
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
# =============================================================================$
# Test of fasterq-dump to process reads/alignments longer than 64k (VDB-6105)
# =============================================================================$
set -e

FASTERQDUMPBIN="$1/fasterq-dump"
RND2SRABIN="$1/rnd2sra"
SANDBOX="LONGREAD_SANDBOX"
ACC="LONGREADS"

mkdir -p $SANDBOX
cd $SANDBOX

# =============================================================================$
# using rnd2sra to create an artificial accession with long reads
# =============================================================================$
$RND2SRABIN --ini stdin --out "$ACC" << EOF
seed = 10101
product = csra
spots = F : 12 : 170000 : 180000
spots = N : 7 : 50 : 100
spots = 1 : 3 : 57 : 102
spots = 2 : 4 : 56 : 103
spotgroup = SG1
EOF
echo "testing for long-reads: sample-accession generated"

# =============================================================================$
# run fasterq-dump to produce FASTQ-files
# =============================================================================$
$FASTERQDUMPBIN $ACC
echo "testing for long-reads: FASTQ-files produced"

# =============================================================================$
# compare produced vs expected MD5-sums of the 2 FASTQ-files
# =============================================================================$
$RND2SRABIN --ini stdin << EOF
product = test
run = T1 T2
T1.exe = :md5 ${ACC}_1.fastq 8c78fbf96bd6b19bd3008854e171c7f4
T2.exe = :md5 ${ACC}_2.fastq 13600705c5119afd4fdba22631777786
EOF
echo "testing for long-reads: success"
echo "-------------------------------------------------------------------------"
cd ..
rm -rf $SANDBOX
