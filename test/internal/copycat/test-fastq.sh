#!/bin/bash

bin_dir=$1
tool_binary=$2

echo "testing ${tool_binary} fastq parser"
mkdir -p actual
rm -f actual/*.out

for f in input/*.fq; do
  fn=$(basename -- "$f")
  tn="${fn%.*}"
  output=$(${bin_dir}/${tool_binary} $f /dev/null | sed -rn 's/^(.*) mtime="([[:print:]]{20,20})"(.*)/\1\3/p' > actual/${tn}.out && diff -q input/${tn}.out actual/${tn}.out)
  res=$?
  if [ "$res" != "0" ];
    then echo "${tool_binary} fastq parser failed, res=${res} output=${output}" && exit 1;
  fi
done

echo "${tool_binary} fastq parser succeeded"
