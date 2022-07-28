#!/bin/bash

bin_dir=$1
sratools=$2

echo "testing expected output for fastq-dump --split-3 via ${sratools}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	${bin_dir}/${sratools} --split-3 SRR390728 2>actual/split_3.stderr ; \
	diff expected/split_3.stderr actual/split_3.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test split_3 via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/split_3.stderr

echo Driver tool test split_3 via ${sratools} is finished