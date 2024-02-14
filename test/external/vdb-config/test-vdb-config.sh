#!/bin/sh

bin_dir=$1

echo "Testing ${bin_dir}/vdb-config... "

output=$(PATH="${bin_dir}:${PATH}" && perl test-vdb-config.pl)

res=$?
if [ "$res" != "0" ];
	then echo "VDB config test FAILED, res=$res output=$output" && exit 1;
fi

echo VDB config test is finished
