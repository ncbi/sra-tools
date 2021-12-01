#! /usr/bin/bash

set -e
#which accession to handle ( this one has sec. alignments )
ACC=$1

#where to put original and copy etc.
TEST_PATH="test_${ACC}/"

#if we do have the TEST-PATH, remove it
if [[ -d $TEST_PATH ]]
then
    echo "removing $TEST_PATH"
    chmod -R +w $TEST_PATH
    rm -rf $TEST_PATH
fi
mkdir $TEST_PATH

#prefetch the accession into the test-path
echo "prefetching the accession"
prefetch $ACC -p -O $TEST_PATH

#un-kar the prefetched accession
SRC_ARCHIVE="$TEST_PATH/$ACC/$ACC.sra"
ORG="${TEST_PATH}org"

echo "kar: $SRC_ARCHIVE ---> $ORG"
kar -x $SRC_ARCHIVE -d $ORG
chmod -R +w $ORG
rm $ORG/lock $ORG/md5

#where to put the copy
DST="${TEST_PATH}copy"

#perform the copy
echo "vdb-copy: $SRC ---> $DST"
vdb-copy $SRC_ARCHIVE $DST -pu

#perform the diff against the original
#DIFF_OPT="--progress --col-by-col"
DIFF_RANGE="-R 1-1000"
DIFF_OPT="--progress $DIFF_RANGE"
DIFF_EX="-x REFERENCE.CMP_BASE_COUNT,SEQUENCE.CMP_BASE_COUNT"
echo "vdb-diff: $SRC <---> $DST"
vdb-diff $SRC_ARCHIVE $DST $DIFF_OPT $DIFF_EX

echo "comparing the file-layout:"
diff -s <(cd $ORG && ls -R1 *) <(cd $DST && ls -R1 *)

#compare the sizes
echo "comparing the sizes"
DST_ARCHIVE="$TEST_PATH/$ACC.archive"
kar -f -c $DST_ARCHIVE -d $DST
SRC_SIZE=`du -sb $SRC_ARCHIVE | awk -F' ' '{print $1}'`
DST_SIZE=`du -sb $DST_ARCHIVE | awk -F' ' '{print $1}'`
PERCENT=$(( $DST_SIZE * 100 / $SRC_SIZE ))

echo "SRC=$SRC_SIZE"
echo "DST=$DST_SIZE"
echo "PERCENT=$PERCENT"

echo "$ACC $PERCENT" >> results.txt

#cleanup
chmod -R +w $TEST_PATH
rm -rf $TEST_PATH

echo "success"
