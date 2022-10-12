#!/bin/bash

bin_dir=$1
tool_binary=$2

echo "testing ${tool_binary} expected output for file name with %"

TEMPDIR=$(cd ${TMPDIR:-/tmp}; pwd)
BAD_NAME="bad%name.txt"
echo "I'm a bad file" > ${TEMPDIR}/${BAD_NAME}

${bin_dir}/${tool_binary} ${TEMPDIR}/${BAD_NAME} /dev/null | grep -q ${BAD_NAME}

res=$?
if [ "$res" != "0" ];
	then echo "${tool_binary} test file name with % failed" && exit 1;
fi

echo "${tool_binary} test file name with % succeeded"
