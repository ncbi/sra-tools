#! /usr/bin/bash

set -e

rm SRR_TST.*

ACC=`./acc.sh`

function f_step1 {
    ./step1.sh
}

function f_step23 {
    ./step2.sh
    ./step3.sh
}

export -f f_step1
export -f f_step23

parallel -j2 ::: f_step1 f_step23

./step4.sh
