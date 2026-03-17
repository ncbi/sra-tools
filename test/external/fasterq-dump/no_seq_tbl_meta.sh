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
# Test of fasterq-dump to handle accession without seq-table-metadata
# =============================================================================$
set -e

FASTERQDUMPBIN="$1/fasterq-dump"
RND2SRABIN="$1/rnd2sra"
SANDBOX="NO_SEQ_TBL_META_SANDBOX"
ACC="NO_SEQ_TBL"

rm -rf $SANDBOX
mkdir -p $SANDBOX
cd $SANDBOX

# =============================================================================$
# using rnd2sra to create an artificial accession without seq-table-metadata
# =============================================================================$
$RND2SRABIN --ini stdin --out $ACC << EOF
seed = 10101
product = flat
write_meta = no
EOF
echo "testing for seq-tbl-no-meta: sample-accession generated"

# =============================================================================$
# run fasterq-dump to produce FASTQ-files
# =============================================================================$
$FASTERQDUMPBIN $ACC
echo "testing for seq-tbl-no-meta: FASTQ-files produced"

# =============================================================================$
# compare produced vs expected MD5-sums of the 2 FASTQ-files
# =============================================================================$
$RND2SRABIN --ini stdin << EOF
product = test
run = T1 T2
T1.exe = :md5 ${ACC}_1.fastq 7487310afa8a9fd021274cf054c38bba
T2.exe = :md5 ${ACC}_2.fastq 21848fed2e4625a4f19f504b41730274
EOF
echo "testing for seq-tbl-no-meta: success"
echo "-------------------------------------------------------------------------"
cd ..
rm -rf $SANDBOX
