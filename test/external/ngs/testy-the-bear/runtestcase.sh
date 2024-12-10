#!/usr/bin/env bash
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

TEST_CMD=$1
CASEID=$2
RC=$3

mkdir -p actual

CMD="$TEST_CMD > \"actual/$CASEID\" 2>&1"
echo $CMD
eval $CMD
rc="$?"

if [ "$rc" != "$RC" ] ; then
    echo "command \"$TEST_CMD\" returned $rc, expected $RC"
    echo "command executed:"
    echo $CMD

	echo "command output:"
    cat actual/$CASEID
    exit 2
fi

diff expected/$CASEID actual/$CASEID
rc="$?"

if [ "$rc" != "0" ] ; then
    exit 3
fi

