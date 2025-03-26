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

# $1 - path sharq binary
# $2 - name of the sharq binary
# $3 - work directory (expected results under expected/, actual results and temporaries created under actual/)
# $4 - test case ID
# $5 - expected result code from sharq
# $6 - telemetry testing (0 - off)
# $7, $8, ... - command line options for sharq
#
# return codes:
# 0 - passed
# 1 - coud not create temp dir
# 2 - unexpected return code from sharq
# 3 - outputs differ (stdout/stderr)

BINDIR=$1
SHARQ_BINARY=$2
WORKDIR=$3
CASEID=$4
RC=$5
TELEMETRY_RPT=$6
shift 6
CMDLINE=$*

DUMP="$BINDIR/vdb-dump"
LOAD="$BINDIR/${SHARQ_BINARY}"
TEMPDIR=$WORKDIR/actual/$CASEID

if [ "$(uname)" == "Darwin" ]; then
    DIFF="diff"
else
    DIFF="diff -Z"
fi

echo "running $CASEID"

if ! test -f ${LOAD}; then
    echo "${LOAD} does not exist. Skipping the test."
    exit 0
fi

mkdir -p $TEMPDIR
rm -rf $TEMPDIR/*
if [ "$?" != "0" ] ; then
    exit 1
fi
export LD_LIBRARY_PATH=$BINDIR/../lib;

if [ "$TELEMETRY_RPT" != "0" ] ; then
CMDLINE="${CMDLINE} -t ${TEMPDIR}/telemetry"
fi

CMD="$LOAD $CMDLINE 1>$TEMPDIR/load.stdout 2>$TEMPDIR/load.stderr"

echo CMD=$CMD
eval $CMD
rc="$?"
if [ "$rc" != "$RC" ] ; then
    echo "$LOAD returned $rc, expected $RC"
    echo "command executed:"
    echo $CMD
    cat $TEMPDIR/load.stderr
    exit 2
fi

expected=$CASEID
suffixes=('-sa-hot' '-sa-cold')

for suffix in "${suffixes[@]}"; do
    if [[ $expected == *"$suffix" ]]; then
        expected=${expected%"$suffix"}
    fi
done

if [ "$rc" == "0" ] ; then
    OUT=stdout
else
    OUT=stderr
    sed -i"" -e '/\[info\]/d' $TEMPDIR/load.$OUT
    sed -i"" -e '/\[info\]/d' $WORKDIR/expected/$expected.$OUT
fi

$DIFF $WORKDIR/expected/$expected.$OUT $TEMPDIR/load.$OUT >$TEMPDIR/diff
rc="$?"
if [ "$rc" != "0" ] ; then

    # retry on sorted output
    sort $WORKDIR/expected/$expected.$OUT >$TEMPDIR/$expected.$OUT.sorted
    sort $TEMPDIR/load.$OUT >$TEMPDIR/load.$OUT.sorted
    $DIFF $TEMPDIR/$expected.$OUT.sorted $TEMPDIR/load.$OUT.sorted >$TEMPDIR/diff.sorted
    rc="$?"
    if (( rc != 0 )); then
        cat $TEMPDIR/diff
        echo "command executed:"
        echo $CMD
        exit 3
    fi
fi

if [ "$TELEMETRY_RPT" != "0" ] ; then
    $DIFF $WORKDIR/expected/$expected.telemetry <(grep -v '"version":' $TEMPDIR/telemetry) >$TEMPDIR/telemetry.diff
    rc="$?"
    if [ "$rc" != "0" ] ; then
        cat $TEMPDIR/telemetry.diff
        echo "command executed:"
        echo $CMD
        exit 3
    fi
fi

exit 0
