#!/bin/bash

test_binary_dir=$1
echo Starting ngs-sdk expected tests, binary directory: ${test_binary_dir}

rm -f actual.txt
echo http_proxy=${http_proxy}

${test_binary_dir}/FragTest ERR225922 10000 2 >> actual.txt
${test_binary_dir}/FastqTableDump ERR225922 2 >> actual.txt
${test_binary_dir}/AlignTest ERR225922 10000 2 >> actual.txt
${test_binary_dir}/AlignSliceTest SRR1121656 1 1 9999 >> actual.txt
${test_binary_dir}/PileupTest SRR1121656 1 9999 10003 >> actual.txt
${test_binary_dir}/RefTest SRR1121656 >> actual.txt
${test_binary_dir}/DumpReferenceFASTA SRR520124 1 >> actual.txt

output=$(diff expected.txt actual.txt)

res=$?
if [ "$res" != "0" ];
	then echo "NGS C++ examples FAILED, res=$res output=$output" && exit 1;
fi

echo "NGS C++ examples work as expected"
rm -f actual.txt
