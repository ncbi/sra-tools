#!/bin/bash
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

# $1 - path to vdb tools (fastq-load.3, vdb-dump)
# $2 - work directory (expected results under expected/, actual results and temporaries created under actual/)
# $3 - test case ID
# $3 - expected result code from fastq-load.3
# $4, $5, ... - command line options for fastq-load.3
#
# return codes:
# 0 - passed
# 1 - coud not create temp dir
# 2 - unexpected return code from fastq-load.3
# 3 - vdb-dump failed on the output of fastq-load.3
# 4 - outputs differ

BINDIR=$1
WORKDIR=$2
CASEID=$3
RC=$4
shift 4
CMDLINE=$*

DUMP="$BINDIR/vdb-dump"
LOAD="$BINDIR/latf-load"
TEMPDIR=$WORKDIR/actual/$CASEID

if [ "$(uname)" == "Darwin" ]; then
    DIFF="diff"
else
    DIFF="diff -Z"
fi

echo "running $CASEID"

mkdir -p $TEMPDIR
rm -rf $TEMPDIR/*
if [ "$?" != "0" ] ; then
    exit 1
fi
export LD_LIBRARY_PATH=$BINDIR/../lib;

#CMD="$LOAD $CMDLINE -o $TEMPDIR/obj --no-user-settings 1>$TEMPDIR/load.stdout 2>$TEMPDIR/load.stderr"
CMD="$LOAD $CMDLINE -o $TEMPDIR/obj 1>$TEMPDIR/load.stdout 2>$TEMPDIR/load.stderr"
    echo $CMD
eval $CMD
rc="$?"
if [ "$rc" != "$RC" ] ; then
    echo "$LOAD returned $rc, expected $RC"
    echo "command executed:"
    echo $CMD
    cat $TEMPDIR/load.stderr
    exit 2
fi

if [ "$rc" == "0" ] ; then
    CMD="$DUMP $TEMPDIR/obj 1>$TEMPDIR/dump.stdout 2>$TEMPDIR/dump.stderr"
    eval $CMD
    rc="$?"
    if [ "$rc" != "0" ] ; then
        echo "command executed:"
        echo $CMD
        cat $TEMPDIR/dump.stderr
        exit 3
    fi
    $DIFF $WORKDIR/expected/$CASEID.stdout $TEMPDIR/dump.stdout >$TEMPDIR/diff
    rc="$?"
else # load failed as expected
    # remove timestamps
    sed -i -e 's/^....-..-..T..:..:.. //g' $TEMPDIR/load.stderr
    # remove pathnames
    sed -i -e 's=/.*/==g' $TEMPDIR/load.stderr
    # remove source locations
    sed -i -e 's=: .*:[0-9]*:[^ ]*:=:=g' $TEMPDIR/load.stderr
    # remove version number
    sed -i -e 's=latf-load\(\.[0-9]*\)*=latf-load=g' $TEMPDIR/load.stderr
    $DIFF $WORKDIR/expected/$CASEID.stderr $TEMPDIR/load.stderr >$TEMPDIR/diff
    rc="$?"
fi
if [ "$rc" != "0" ] ; then
    cat $TEMPDIR/diff
    echo "command executed:"
    echo $CMD
    exit 4
fi

#rm -rf $TEMPDIR

exit 0
