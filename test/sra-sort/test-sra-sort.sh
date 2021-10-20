#!/bin/bash

bin_dir=$1

echo Testing sra-sort from ${bin_dir}

rm -rf tmp_dir
mkdir -p tmp_dir

output=$(PATH="${bin_dir}:${PATH}" sh ./md-created.sh)
res=$?

#rm -rf tmp_dir
if [ "$res" != "0" ];
	then echo "sra-sort test FAILED, res=$res output=$output" && exit 1;
fi

echo "$output"
