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

TEST_CMD=$1
CASEID=$2
RC=$3

CMD="$TEST_CMD > \"actual/$CASEID.tmp\" 2>&1"
#echo $CMD
eval $CMD
rc="$?"

if [ "$rc" != "$RC" ] ; then
    echo "command \"$TEST_CMD\" returned $rc, expected $RC"
    echo "command executed:"
    echo $CMD

	echo "command output:"
    cat actual/$CASEID.tmp
    exit 2
fi

# remove first two columns from output: datetime and progname 
cat "actual/$CASEID.tmp" | awk '{if(substr($2,1,12) == "vdb-validate"){$2=$1="";} print $0}' \
    | perl -e "while(<>) { s|'.*TEST-DATA/|'| ; print }" > "actual/$CASEID"
rm "actual/$CASEID.tmp"

# remove trailing white spaces
sed -i -e 's/^[ \t]*//g' "actual/$CASEID"
# remove file names and line numbers
sed -i -e 's/: .*:[0-9]*:[^ ]*:/:/g' "actual/$CASEID"

diff expected/$CASEID actual/$CASEID
rc="$?"

if [ "$rc" != "0" ] ; then
    exit 3
fi

