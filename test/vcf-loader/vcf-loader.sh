#!/bin/bash

test_bin=$1
vdb_src_interfaces=$2

echo "testing vcf-loader: ${test_bin} with vdb_src_interfaces: ${vdb_src_interfaces}"

echo "vdb/schema/paths = \"${vdb_src_interfaces}\"" > tmp.kfg
VDB_CONFIG=.

output=$(${test_bin})

res=$?
if [ "$res" != "0" ];
	then echo "vcf-loader test FAILED, res=$res output=$output" && exit 1;
fi

echo vcf-loader is finished
