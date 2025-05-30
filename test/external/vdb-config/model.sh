#!/bin/sh

test_bin=$1

echo "testing ${test_bin}"

output=$(NCBI_SETTINGS=$(pwd)/tmp/u \
	${test_bin} && \
	rm -r $(pwd)/tmp)

res=$?
if [ "$res" != "0" ];
	then echo "VDB Copy model test FAILED, res=$res output=$output" && exit 1;
fi

echo VDB Copy model test is finished
