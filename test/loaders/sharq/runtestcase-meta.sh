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

# load a VDB database and verify metadata

# $1 - pathname of the loader
# $2 - pathname of kdbmeta
# $3 - work directory (expected results under expected/, actual results and temporaries created under actual/)
# $4 - test case ID
# $5 - command line options for the loader
# $6 - command line options for kdbmeta

# return codes:
# 0 - passed
# 1 - coud not create temp dir
# 2 - unexpected return code from loader
# 3 - unexpected return code from kdbmeta
# 4 - kdbmeta outputs differ

LOAD_BINARY=$1
KDBMETA_BINARY=$2
WORKDIR=$3
CASEID=$4
LOAD_ARGS=$5
KDBMETA_ARGS=$6

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

CMD="$LOAD_BINARY ${LOAD_ARGS} 2>$TEMPDIR/load.stderr | general-loader -T $TEMPDIR/db -I $WORKDIR/../../../libs/schema:$WORKDIR/../../../../ncbi-vdb/interfaces 1>$TEMPDIR/load.stdout 2>>$TEMPDIR/load.stderr"

echo CMD=$CMD
eval $CMD
rc="$?"
if [ "$rc" != "0" ] ; then
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

CMD=${KDBMETA_BINARY} $TEMPDIR/db ${KDBMETA_ARGS}  | grep -v timestamp >$TEMPDIR/meta
$DIFF $WORKDIR/expected/$expected.meta $TEMPDIR/meta >$TEMPDIR/meta.diff

echo CMD=$CMD
eval $CMD
rc="$?"
if [ "$rc" != "0" ] ; then
    cat $TEMPDIR/meta.diff
    echo "command executed:"
    echo $CMD
    exit 3
fi

exit 0
