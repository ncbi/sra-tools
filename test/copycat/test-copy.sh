#!/bin/bash

bin_dir=$1
FILE="1.xml"

echo "testing copycat copy"
mkdir -p actual
rm -rf actual/${FILE}

output=$(${bin_dir}/copycat input/${FILE} actual/ && diff -q input/${FILE} actual/${FILE})

res=$?
if [ "$res" != "0" ];
	then echo "copycat copy failed, res=${res} output=${output}" && exit 1;
fi

echo "copycat copy succeeded"
