#!/bin/sh
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
#echo "$0 $*"

# $1 - path to sra tools (sra-pileup, ngs-pileup)
# $2 - work directory (actual results and temporaries created under actual/)
# $3 - test case ID
# $4, $5, ... - command line options for pileup programs 
#
# return codes:
# 0 - passed
# 1 - could not create temp dir
# 2 - unexpected return code from sra-pileup 
# 3 - unexpected return code from ngs-pileup 
# 4 - outputs differ

BINDIR=$1
BINARY_SUFFIX=$2
WORKDIR=$3
CASEID=$4
shift 4
CMDLINE=$*

SRA_PILEUP="$BINDIR/sra-pileup" # sra-pileup is not the target of the test
NGS_PILEUP="$BINDIR/ngs-pileup${BINARY_SUFFIX}"
TEMPDIR=$WORKDIR/actual/$CASEID

printf "running $CASEID: "

export NCBI_SETTINGS=/

mkdir -p $TEMPDIR
rm -rf $TEMPDIR/*
if [ "$?" != "0" ] ; then
    exit 1
fi

CMD="$SRA_PILEUP $CMDLINE 1>$TEMPDIR/sra.stdout.tmp 2>$TEMPDIR/sra.stderr"
printf "sra... "
eval "$CMD"
if [ "$?" != "0" ] ; then
    echo "SRA pileup failed. Command executed:"
    echo $CMD
    cat $TEMPDIR/sra.stderr
    exit 2
fi    
cut -f 1,2,4 $TEMPDIR/sra.stdout.tmp >$TEMPDIR/sra.stdout 
   
CMD="$NGS_PILEUP $CMDLINE 1>$TEMPDIR/ngs.stdout 2>$TEMPDIR/ngs.stderr"
printf "ngs... "
eval "$CMD"
if [ "$?" != "0" ] ; then
    echo "NGS pileup failed. Command executed:"
    echo $CMD
    cat $TEMPDIR/ngs.stderr
    exit 3
fi    

printf "diff... "
diff $TEMPDIR/sra.stdout $TEMPDIR/ngs.stdout >$TEMPDIR/diff
if [ "$?" != "0" ] ; then
    cat $TEMPDIR/diff
    echo "command executed:"
    echo $CMD
    exit 4
fi    

printf "done\n"
rm -rf $TEMPDIR

exit 0
