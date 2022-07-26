#!/bin/bash

bin_dir=$1
sratools=$2

echo "testing expected output for fastq-dump --help via ${sratools}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_IMPERSONATE=fastq-dump \
	${bin_dir}/${sratools} --help | sed -e'/"fastq-dump" version/ s/version.*/version <deleted>/' >actual/help_fastq-dump.stdout ; \
	diff expected/help_fastq-dump.stdout actual/help_fastq-dump.stdout)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test help_fastq-dump via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/help_fastq-dump.stdout

echo Driver tool test help_fastq-dump via ${sratools} is finished