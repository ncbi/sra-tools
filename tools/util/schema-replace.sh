#!/bin/bash

execute()
{
    echo $1
    eval $1
}

SRC=$1
DST=$2
TMP=$3
SCHEMA="align/align.vschema"
DB_TYPE="NCBI:align:db:alignment_sorted#1"
TAB_TYPE="NCBI:align:tbl:align_sorted#1"

[ $# -gt 3 ] && SCHEMA=$4

execute "kar -fx $SRC -d $TMP" || exit $?
execute "chmod +w -R $TMP" || exit $?
execute "schema-replace -s $SCHEMA -d $DB_TYPE -t $TAB_TYPE $TMP" || exit $?
execute "kar -fd $TMP -c $DST" || exit $?
execute "rm -rf $TMP"
