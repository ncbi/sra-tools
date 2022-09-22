#! /usr/bin/bash

set -e

ACC=`./acc.sh`
VDBDUMP=`./vdb-dump.sh`

function compile {
    g++ -o $1 -O3 $1.cpp
}

compile "step2"

CMD="$VDBDUMP $ACC -T REF -C NAME,SEQ_LEN -f tab | ./step2 > SRR_TST.ref_lookup"
#echo $CMD
eval "$CMD"
