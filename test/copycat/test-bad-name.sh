#!/bin/bash

bin_dir=$1

echo "testing expected output for file name with %"

TEMPDIR=$(cd ${TMPDIR:-/tmp}; pwd)
BAD_NAME="bad%name.txt"
echo "I'm a bad file" > ${TEMPDIR}/${BAD_NAME}

${bin_dir}/copycat ${TEMPDIR}/${BAD_NAME} /dev/null | grep -q ${BAD_NAME}

res=$?
if [ "$res" != "0" ];
	then echo "copycat test file name with % failed" && exit 1;
fi

echo "copycat test file name with % succeeded"
