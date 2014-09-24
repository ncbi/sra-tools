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

# wait for a file to become available (used in remote builds to handle delays introduced by the network file system)
# $1 - file to wait for
# $2 - timeout (seconds), optional, default 120s
# return codes: 
#   0 ok, 
#   1 timed out, 
#   2 no file specified, 
#   3 timeout not numeric

if [ "$1" != "" ] 
then
    FILE=$1
else
    echo "$0 $*: no file specified" >&2
    exit 2
fi     

if [ "$2" == "" ] 
then 
    TIMEOUT=120
elif [[ $2 =~ ([0-9])+ ]]
then 
    TIMEOUT=$2
else
    echo "$0 $*: timeout not a number" >&2
    exit 3
fi

for (( i=0; i < ${TIMEOUT}; i++ ))
do
    # give nfs a nudge
    ls $(dirname $FILE) >/dev/null  
    if [ -e ${FILE} ]
    then
        exit 0
    fi 
    echo "waiting for '$FILE' ..."
    sleep 1s
done
exit 1
