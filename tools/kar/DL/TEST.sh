#!/bin/bash

#####
## This test script
#####

DD=`dirname $0`

BIN_D=$( cd $( dirname $0 ); pwd )
BASE_D=$( dirname $BIN_D )

TEST_D=$BASE_D/test
SCM_D=$BASE_D/schema

run_cmd ()
{
    ARA="$@"
    if [ -z "$ARA" ]
    then
        echo WARNING: no arguments passed to run_cmd >&2
        return
    fi

    echo
    echo "########"
    echo "##  $ARA"
    eval $ARA
    RETC=$?
    if [ $RETC -ne 0 ]
    then
        echo "ERROR: command failed ($RETC)" >&2
        exit $RETC
    fi
}

if [ ! -d "$SCM_D" ]
then
    echo ERROR: can not stat schema directory \'$SCM_D\' >&2
    exit 1
fi

if [ ! -d "$TEST_D" ]
then
    run_cmd mkdir $TEST_D
fi

echo

if [ $# -ne 1 ]
then
    echo Usage : $( basename $0 ) ACCESSION >&2
    exit 1
fi

ACCESSION=$1
DST_D=$TEST_D/$ACCESSION

echo DELITE: $ACCESSION to $DST_D

##
## 1 Exporting
run_cmd $BIN_D/sra_delite.sh import --accession $ACCESSION --target $DST_D --force

##
## 2 Deliting
run_cmd $BIN_D/sra_delite.sh delite --schema $SCM_D --target $DST_D

##
## 3 Exporting
run_cmd $BIN_D/sra_delite.sh export --target $DST_D --force

echo ENJOY!

