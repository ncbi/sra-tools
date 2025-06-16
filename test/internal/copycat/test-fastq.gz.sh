#!/bin/bash

bin_dir=$1
tool_binary=$2

echo "testing ${tool_binary} nested fastq parser"

if ! test -f ${bin_dir}/${tool_binary}; then
    echo "${bin_dir}/${tool_binary} does not exist. Skipping the test."
    exit 0
fi

mkdir -p actual
rm -f actual/*.out

output=$(${bin_dir}/${tool_binary} input/3.fq.gz /dev/null | sed -rn 's/^(.*) mtime="([[:print:]]{20,20})"(.*)/\1\3/p' > actual/3.out && diff -q input/3.out actual/3.out)
res=$?
if [ "$res" != "0" ];
    then echo "${tool_binary} fastq parser failed, res=${res} output=${output}" && exit 1;
fi

echo "${tool_binary} nested fastq parser succeeded"
