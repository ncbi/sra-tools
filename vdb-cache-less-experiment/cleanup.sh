#! /usr/bin/bash

set -e

rm SRR_TST.* step1 step2 step3 step4

ACC=`./acc.sh`
rm -rf $ACC
