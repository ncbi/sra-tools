#!/bin/bash

test_bin_dir=$1
echo test_bin_dir: ${test_bin_dir}

echo "testing sanitizers"

echo "asan with a memory access error should fail..."
output=$(${test_bin_dir}/asan 2>&1)
res=$?
if [ "$res" == "0" ];
	then echo "asan FAILED, res=$res output=$output" && exit 1;
fi

echo "asan with no memory access error should not fail..."
output=$(${test_bin_dir}/asan 0 2>&1)
res=$?
if [ "$res" != "0" ];
	then echo "asan with no error FAILED, res=$res output=$output" && exit 1;
fi

echo "asan with disabled instrumentation and with a memory access error should not fail..."
output=$(${test_bin_dir}/asan_disabled 2>&1)
res=$?
if [ "$res" != "0" ];
	then echo "asan_disabled FAILED, res=$res output=$output" && exit 1;
fi

echo "asan with disabled instrumentation and with no memory access error should not fail..."
output=$(${test_bin_dir}/asan_disabled 0 2>&1)
res=$?
if [ "$res" != "0" ];
	then echo "asan_disabled with no error FAILED, res=$res output=$output" && exit 1;
fi

