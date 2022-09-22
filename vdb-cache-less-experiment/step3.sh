#! /usr/bin/bash

set -e

ACC=`./acc.sh`
VDBDUMP=`./vdb-dump.sh`

function compile {
    g++ -o $1 -O3 $1.cpp
}

compile "step3"

CMD="$VDBDUMP $ACC -T PRIM -C GLOBAL_REF_START,REF_ORIENTATION,REF_LEN -f tab | ./step3 SRR_TST"
#echo $CMD
eval $CMD
