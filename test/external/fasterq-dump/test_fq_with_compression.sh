#! /bin/sh

# ================================================================
#
#   The purpose of this test is to verify that the new
#   compression options for fasterq-dump "--gzip" and "--bzip2"
#   are working and produce the same result as the the plain
#   output.
#
# ================================================================

set -e

BINDIR="$1"

if [ ! -d $BINDIR ]; then
    echo "cannot find ${BINDIR} --> exit"
    exit 3
fi

TOOL="$BINDIR/fasterq-dump"

if [ ! -x $TOOL ]; then
    echo "cannot find executable ${TOOL} --> exit"
    exit 3
fi

SUBDIR="T1"

rm -rf $SUBDIR
mkdir -p $SUBDIR
cd $SUBDIR

ACC="../ERR3487613"

if [ ! -f $ACC ]; then
    echo "cannot find ${ACC} --> exit"
    exit 3
fi

make_plain() {
    $TOOL -o out1 $ACC
}

make_gz() {
    $TOOL --gzip -o out2 $ACC
    gzip -d out2_1.fastq.gz
    gzip -d out2_2.fastq.gz
    diff -s out1_1.fastq out2_1.fastq
    diff -s out1_2.fastq out2_2.fastq
}

make_bz2() {
    $TOOL --bzip2 -o out3 $ACC
    bzip2 -d out3_1.fastq.bz2
    bzip2 -d out3_2.fastq.bz2
    diff -s out1_1.fastq out3_1.fastq
    diff -s out1_2.fastq out3_2.fastq
}

make_plain
make_gz
make_bz2

echo "success!"

cd ..
rm -rf $SUBDIR
