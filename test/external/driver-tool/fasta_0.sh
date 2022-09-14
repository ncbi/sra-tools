#!/bin/bash

bin_dir=$1
sratools=$2

echo "testing expected output for fastq-dump --fasta 0 <run> via ${sratools}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	${bin_dir}/${sratools} --fasta 0 SRR390728 2>actual/fasta_0.stderr ; \
	diff expected/fasta_0.stderr actual/fasta_0.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test fasta_0 via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/fasta_0.stderr

echo Driver tool test fasta_0 via ${sratools} is finished