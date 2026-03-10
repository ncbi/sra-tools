#!/bin/bash

bin_dir=$1
tool_binary=$2

echo "testing ${tool_binary} fasta parser"

if ! test -f ${bin_dir}/${tool_binary}; then
    echo "${bin_dir}/${tool_binary} does not exist. Skipping the test."
    exit 0
fi

mkdir -p actual
rm -f actual/*.out

output=$(${bin_dir}/${tool_binary} input/5.fsta /dev/null | sed -rn 's/^(.*) mtime="([[:print:]]{20,20})"(.*)/\1\3/p' > actual/5.out && diff -q input/5.out actual/5.out)
res=$?
if [ "$res" != "0" ];
    then echo "${tool_binary} fasta parser failed, res=${res} output=${output}" && exit 1;
fi

output=$(${bin_dir}/${tool_binary} input/6.f /dev/null | sed -rn 's/^(.*) mtime="([[:print:]]{20,20})"(.*)/\1\3/p' > actual/6.out && diff -q input/6.out actual/6.out)
res=$?
if [ "$res" != "0" ];
    then echo "${tool_binary} fasta parser failed, res=${res} output=${output}" && exit 1;
fi


echo "${tool_binary} fasta parser succeeded"
