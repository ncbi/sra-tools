#! /usr/bin/bash

set -e

ACC=`./acc.sh`
VDBDUMP=`./vdb-dump.sh`

function compile {
    g++ -o $1 $1.cpp
}

compile "step4"

TBL="-T PRIM"
COLUMNS="-C SEQ_SPOT_ID,MAPQ,CIGAR_SHORT,READ"
#RANGE="-R 1-100000"
FMT="-f tab"

CMD="$VDBDUMP $ACC $TBL $COLUMNS $RANGE $FMT | ./step4 SRR_TST > SRR_TST.SAM"
#echo $CMD
eval $CMD
