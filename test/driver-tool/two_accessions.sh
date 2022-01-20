#!/bin/bash

bin_dir=$1
test_name=$2

echo "testing expected output for two accessions <run1> <run2>"

mkdir -p actual

output=$(NCBI_SETTINGS=tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_DRY_RUN=1 \
	SRATOOLS_IMPERSONATE=vdb-dump \
	${bin_dir}/sratools SRR000001 SRR390728 2>actual/${test_name}.stderr ; \
	diff expected/${test_name}.stderr actual/${test_name}.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test ${test_name} FAILED, res=$res output=$output" && exit 1;
fi

echo "Driver tool test ${test_name} is finished"
