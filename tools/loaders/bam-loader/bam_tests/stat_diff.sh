#!/bin/bash

edit_stats()
{
    local in_file=$1
    local out_file=$2
    sed 's/<Run accession="[^"]*"/<Run accession=""/g' ${in_file}  | sed 's/<Size value="[^"]*"/<Size value=""/g' > ${out_file}
}

progname=`basename $0`
[ ! -f "$1" ] && { printf "${progname}: First file doesn't exist (error:1)\n"; exit 1; }
[ ! -f "$2" ] && { printf "${progname}: Second file doesn't exist (error:2)\n"; exit 1; }

trap '[ -f "$TMPFILE1" ] && rm "$TMPFILE1"; [ -f "$TMPFILE2" ] && rm "$TMPFILE2";' EXIT
TMPFILE1=$(mktemp) || exit 1
TMPFILE2=$(mktemp) || exit 1

edit_stats $1 $TMPFILE1
edit_stats $2 $TMPFILE2
diff $TMPFILE1 $TMPFILE2

exit $?