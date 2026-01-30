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

# $1 - path to executables
# $2 - work directory (expected results under expected/, actual results and temporaries created under actual/)
# $3 - test case ID
#
# return codes:
# 0 - passed
# 1 - coud not create temp dir
# 2 - non-0 return code from the tool
# 3 - outputs differ

BINDIR=$1
WORKDIR=$2
CASEID=$3
RC=0

TOOL_ARGS=$WORKDIR/$TEMPDIR/$CASEID

TEMPDIR=$WORKDIR/actual/$CASEID
STDOUT=$TEMPDIR/stdout
STDERR=$TEMPDIR/stderr

DIFF="diff -b"
if [ "$(uname -s)" = "Linux" ] ; then
    if [ "$(uname -o)" = "GNU/Linux" ] ; then
        DIFF="diff -b -Z"
    fi
fi

echo "running $CASEID"

mkdir -p $TEMPDIR
rm -rf $TEMPDIR/*
if [ "$?" != "0" ] ; then
    exit 1
fi

run_cmd() {
    CMD="$* 1>>$STDOUT 2>>$STDERR"
    echo $CMD
    eval $CMD
    rc="$?"
    if [ "$rc" != "0" ] ; then
        # check if this error is expected
        if [ -f $WORKDIR/expected/$CASEID.stderr ] ; then
            $DIFF $WORKDIR/expected/$CASEID.stderr $STDERR >$TEMPDIR/diff
            rc="$?"
        fi
        if [ "$rc" != "0" ] ; then
            echo $CMD
            echo "returned $rc"
            cat $STDERR
        fi
        # exit the script with 0 if the stderr was as expected
        exit $rc
    fi
}

run_cmd ${BINDIR}/kar -x input/$CASEID.sra -d $TEMPDIR/$CASEID
run_cmd ${BINDIR}/sra-add-fp $TEMPDIR/$CASEID
run_cmd ${BINDIR}/fastq-dump $TEMPDIR/$CASEID --split-spot -O $TEMPDIR
run_cmd "kdbmeta -X t $TEMPDIR/$CASEID -T SEQUENCE QC/current/fingerprint | sed s'|<fingerprint>||g' | sed s'|</fingerprint>||g'"

$DIFF $WORKDIR/expected/$CASEID.stdout $STDOUT >$TEMPDIR/diff
rc="$?"
if [ "$rc" != "0" ] ; then
    if [ -f $WORKDIR/expected/$CASEID.stderr ] ; then
        $DIFF $WORKDIR/expected/$CASEID.stderr $STDERR >$TEMPDIR/diff
        rc="$?"
    fi
    if [ "$rc" != "0" ] ; then
        cat $TEMPDIR/diff
        exit 3
    fi
fi

rm -rf $TEMPDIR

exit 0
