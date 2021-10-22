#!/bin/bash

bin_dir=$1

echo "testing expected output for fastq-dump --split-e (sic)"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	${bin_dir}/sratools --split-e SRR390728 2>actual/split_e.stderr ; \
	diff expected/split_e.stderr actual/split_e.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test split_e FAILED, res=$res output=$output" && exit 1;
fi

echo Driver tool test split_e is finished