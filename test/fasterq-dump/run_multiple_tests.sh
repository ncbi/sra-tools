#!/bin/bash

set -e

ARGFILE="accessions.txt"
CMD="./handle_one.sh"
MAXPROCS="2"

cd fq_tests
xargs --arg-file=$ARGFILE --max-procs=$MAXPROCS --max-args=1 -d\\n $CMD
