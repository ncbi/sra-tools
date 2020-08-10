#! /bin/bash

SANDBOX="TEST_ON_SRA_EXT_SANDBOX"
ACC="SRR000001"
ACC_C="SRR000001.sra"

mkdir -p $SANDBOX
cd $SANDBOX

#create a partial copy of an accession if not already there
if [ ! -f $ACC_C ]; then
    echo "creating partial local copy of $ACC"
    vdb-copy $ACC acc_part -R1-1000 -f
    echo "partial local copy created"

    echo "creating single file from partial copy of $ACC"
    kar -c $ACC_C -d ./acc_part -f
    echo "single file created"

    rm -rf ./acc_part
    echo "local temp. directory removed"
fi

echo "run fasterq-dump run on local single file : '$ACC_C'"
fasterq-dump $ACC_C -f
echo "fasterq-dump run on local copy"

#test that the 3 expected output-files do exist
#we are expecting:
#   SRR000001_1.fastq
#   SRR000001_2.fastq
#   SRR000001.fastq

FASTQ1="${ACC}_1.fastq"
FASTQ2="${ACC}_2.fastq"
FASTQ3="${ACC}.fastq"

declare -i CNT=0

if [ -f $FASTQ1 ]; then
    echo "$FASTQ1 found"
    CNT=$CNT+1
else
    echo "$FASTQ1 NOT found"
fi

if [ -f $FASTQ2 ]; then
    echo "$FASTQ2 found"
    CNT=$CNT+1
else
    echo "$FASTQ2 NOT found"
fi

if [ -f $FASTQ3 ]; then
    echo "$FASTQ3 found"
    CNT=$CNT+1
else
    echo "$FASTQ3 NOT found"
fi

echo "we found $CNT of the 3 expected output-files"

cd ..

if (( $CNT == 3 )); then
    echo "success!"
    rm -rf $SANDBOX
    exit 0
else
    echo "error! we found these files instead"
    ls -l ./$SANDBOX
    exit 3
fi
