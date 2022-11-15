#!/bin/bash

if [ $# -ne 2 ]
then
cat <<EOF >&2

That script will test vdb-dumb utility for buffer unsufficient but ( VDB-3937 )

Syntax : `basename $0` vdb-dump-path kar-archive-path

where :
           vdb-dump-path - path to testing utility
        kar-archive-path - path to archive with data causing error

EOF

exit 1
fi

VDB_D=$1
KAR_F=$2

if [ ! -x "$VDB_D" ]
then
    echo Can not stat executable \'$VDB_D\' >&2
    exit 1
fi

if [ ! -f "$KAR_F" ]
then
    echo Can not stat file \'$KAR_F\' >&2
    exit 1
fi

echo "TEST: buffer insufficient error (VDB-3937)"

SIG_S="buffer insufficient while converting string within text module"
PASS_S="PASS"

RET=`$VDB_D $KAR_F 2>&1 1>/dev/null | grep "$SIG_S" || echo $PASS_S`

if [ "$RET" != $PASS_S ]
then
    echo TEST: FAILED
    exit 1
fi

echo TEST: PASSED
exit 0
