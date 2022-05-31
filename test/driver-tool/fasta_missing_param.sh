#!/bin/bash

bin_dir=$1
sratools=$2

echo "testing expected output for fastq-dump --fasta <run> via ${sratools}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	${bin_dir}/${sratools} --fasta SRR390728 2>actual/fasta_missing_param.stderr ; \
	diff expected/fasta_missing_param.stderr actual/fasta_missing_param.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test fasta_missing_param via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/fasta_missing_param.stderr

echo Driver tool test fasta_missing_param via ${sratools} is finished