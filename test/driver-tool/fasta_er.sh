#!/bin/bash

bin_dir=$1

echo "testing expected output for fasterq-dump" ${3} "<run>"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fasterq-dump \
	${bin_dir}/sratools ${3} SRR390728 2>actual/${2}.stderr ; \
	diff expected/${2}.stderr actual/${2}.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test ${2} FAILED, res=$res output=$output" && exit 1;
fi

echo "Driver tool test ${2} is finished"
