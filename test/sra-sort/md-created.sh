#!/bin/bash

TMP=/export/home/TMP
if [ ! -d "$TMP" ] ; then
    echo $TMP is not found: skipping the test
    exit 0
fi

if [ "$TEST_DATA" == "" ] ; then
    echo TEST_DATA is not set: exiting
    exit 1
fi

I=`whoami`
DSTDIR=$TMP/$I/sra-sort-md
DST=$DSTDIR/sorted

rm -fr $DSTDIR

SORT=sra-sort
which $SORT > /dev/null 2>&1
if [ "$?" != "0" ] ; then
    echo "sra-sort not found: add it to your PATH"
    exit 10
fi

OPT="--tempdir $DSTDIR --mmapdir $DSTDIR --map-file-bsize 80000000000 --max-ref-idx-ids 4000000000 --max-idx-ids 4000000000 --max-large-idx-ids 800000000"
SRC=$TEST_DATA/SRR5318091-sra-sort-md
CMD="$SORT -f -v -L6 $OPT $SRC $DST"
echo $ $CMD

$CMD
EXIT=$?
if [ $EXIT -ne 0 ] ; then
    echo "sra-sort failed with $EXIT"
    rm -r $DSTDIR
    exit $EXIT
fi

if [ ! -d "$DST/md" ] ; then
    echo Failure: md was not created in $DST:
    echo $ ls $DST
    ls $DST
    rm -r $DSTDIR
    exit 20
else
    echo Success: md was created in $DST:
    echo $ ls $DST
    ls $DST
fi

rm -r $DSTDIR
