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

# $1 - path to sra-tools executables (general-loader, vdb-dump)
# $2 - name of the dumper executable (vdb-dump, kdbmeta etc.)
# $3 - work directory (expected results under expected/, actual results and temporaries created under actual/)
# $4 - test case ID (expect a file input/$3.gl to exist)
# $5 - expected return code
# $6 - command line options for general-loader
# $7 - command line options for dumper
#
# return codes:
# 0 - passed
# 1 - could not create temp dir
# 2 - unexpected return code from general-loader
# 3 - vdb-dump failed on the output of general-loader
# 4 - outputs differ

BINDIR=$1
DUMPER=$2
WORKDIR=$3
CASEID=$4
RC=$5
LOAD_OPTIONS=$6
DUMP_OPTIONS=$7

DUMP="$BINDIR/$DUMPER"
LOAD="$BINDIR/general-loader"
TEMPDIR=$WORKDIR/actual/$CASEID

echo "running test case $CASEID"

mkdir -p $TEMPDIR
if [ "$?" != "0" ] ; then
    echo "cannot create "
    exit 1
fi
rm -rf $TEMPDIR/*

CMD="cat input/$CASEID.gl | $LOAD $LOAD_OPTIONS 1>$TEMPDIR/load.stdout 2>$TEMPDIR/load.stderr"
#echo $CMD
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
    CMD="$DUMP $TEMPDIR/db $DUMP_OPTIONS 1>$TEMPDIR/dump.stdout 2>$TEMPDIR/dump.stderr"
    #echo $CMD
    eval $CMD || ( echo "$CMD" && exit 3 )
    
    # remove timestamps, date from metadata
    sed -i -e 's/<timestamp>.*<\/timestamp>/<timestamp\/>/g' $TEMPDIR/dump.stdout
    sed -i -e 's/date=".*" name/date="" name/g' $TEMPDIR/dump.stdout
    
    diff $WORKDIR/expected/$CASEID.stdout $TEMPDIR/dump.stdout >$TEMPDIR/diff
    rc="$?"
else
    # remove timestamps
    sed -i -e 's/^....-..-..T..:..:.. //g' $TEMPDIR/load.stderr
    # remove pathnames
    sed -i -e 's=/.*/==g' $TEMPDIR/load.stderr
    # remove source locations
    sed -i -e 's=: .*:[0-9]*:[^ ]*:=:=g' $TEMPDIR/load.stderr
    # remove version number
    sed -i -e 's=general-loader\(\.[0-9]*\)*=general-loader=g' $TEMPDIR/load.stderr
    diff $WORKDIR/expected/$CASEID.stderr $TEMPDIR/load.stderr >$TEMPDIR/diff
    rc="$?"
fi

if [ "$rc" != "0" ] ; then
    cat $TEMPDIR/diff
    echo "command executed:"
    echo $CMD
    exit 4
fi    

rm -rf $TEMPDIR

exit 0
