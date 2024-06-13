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

echo "$0 $*"

#
# A generic script to run a command line tool and diff the output against previously saved
#
# $1 - the executable
# $2 - test case ID
#
# Options:
# -d|--dir <path>       (default .) work directory (expected results under expected/, actual results and temporaries created under actual/)
#                           expected/$2.stdout is expected to exists and will be diffed against the sdtout of the run
#                           if expected/$2.stderr exists, it is diffed against the sdterr of the run
# -r|--rc <number>      (default 0) expected return code
# -a|--args <string>    (deault empty) arguments to pass to the executable, e.g. --args "ACGTAGGGTCC --threads 2"
# -s|--sort             (default off) sort the tool's stdout
#
# return codes:
# 0 - passed
# 1 - could not create $WORKDIR/actual/$2/
# 2 - unexpected return code from the executable
# 3 - stdouts differ
# 4 - stderrs differ
# 5 - unknown option specified

EXE=$1
CASEID=$2
shift 2

WORKDIR="."
RC=0
ARGS=""
SORT=""

while [[ $# > 1 ]]
do
    key="$1"
    case $key in
        -d|--dir)
            WORKDIR="$2"
            shift
            ;;
        -r|--rc)
            RC="$2"
            shift
            ;;
        -a|--args)
            ARGS="$2"
            shift
            ;;
        -s|--sort)
            SORT="1"
            ;;
        *)
            echo "unknown option " $key
            exit 5
        ;;
    esac
    shift # past argument or value
done

TEMPDIR=$WORKDIR/actual
EXPECTED_STDOUT=$WORKDIR/expected/$CASEID.stdout
EXPECTED_STDERR=$WORKDIR/expected/$CASEID.stderr
ACTUAL_STDOUT=$TEMPDIR/$CASEID.stdout
ACTUAL_STDERR=$TEMPDIR/$CASEID.stderr

echo -n "running test case $CASEID... "

mkdir -p $TEMPDIR
if [ "$?" != "0" ] ; then
    echo "failed"
    echo "cannot create $TEMPDIR"
    exit 1
fi
rm -rf $TEMPDIR/$CASEID*

if [ "$SORT" == "" ] ; then
    CMD="$EXE $ARGS 1>$ACTUAL_STDOUT 2>$ACTUAL_STDERR"
else
    CMD="$EXE $ARGS 2>$ACTUAL_STDERR | sort >$ACTUAL_STDOUT"
fi

export NCBI_SETTINGS=/

#echo $CMD
eval $CMD
rc="$?"
if [ "$rc" != "$RC" ] ; then
    echo "failed"
    echo "$EXE returned $rc, expected $RC"
    echo "command executed:"
    echo $CMD
    cat $ACTUAL_STDOUT
    cat $TEMPDIR/$CASEID.stderr
    exit 2
fi

# remove version number from the actual output
SED="sed -i -e 's=$(basename $EXE) : \(.[0-9]*\)*==g' $ACTUAL_STDOUT"
eval $SED

diff --ignore-blank-lines $EXPECTED_STDOUT $ACTUAL_STDOUT >$TEMPDIR/$CASEID.stdout.diff
rc="$?"
if [ "$rc" != "0" ] ; then
    echo -e "\ndiff $EXPECTED_STDOUT $ACTUAL_STDOUT failed with $rc"
    cat $TEMPDIR/$CASEID.stdout.diff  >&2
    echo "command executed:"
    echo $CMD
    exit 3
fi

if [ -f $EXPECTED_STDERR ]
    then
    # clean up stderr:
    #
    # remove timestamps
    sed -i -e 's/^....-..-..T..:..:.. //g' $ACTUAL_STDERR
    # remove pathnames
    sed -i -e 's=/.*/==g' $ACTUAL_STDERR
    # remove source locations
    sed -i -e 's=: .*:[0-9]*:[^ ]*:=:=g' $ACTUAL_STDERR
    # remove version number if present
    sed -i -e 's=$(basename $EXE)\(\.[0-9]*\)*=$(basename $EXE)=g' $ACTUAL_STDERR
    #

    diff $EXPECTED_STDERR $ACTUAL_STDERR >$TEMPDIR/$CASEID.stderr.diff
    rc="$?"
    if [ "$rc" != "0" ] ; then
        echo -e "\ndiff $EXPECTED_STDERR $ACTUAL_STDERR failed with $rc"
        cat $TEMPDIR/$CASEID.stderr.diff >&2
        echo "command executed:"
        echo $CMD
        exit 4
    fi
fi

rm -rf $TEMPDIR/$CASEID*

echo "passed"

exit 0
