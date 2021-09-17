#!/bin/bash

bin_dir=$1

echo "testing expected output for fastq-dump --fasta <number> <run>"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	${bin_dir}/sratools --fasta 75 SRR390728 2>actual/fasta_number.stderr ; \
	diff expected/fasta_number.stderr actual/fasta_number.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test fasta_number FAILED, res=$res output=$output" && exit 1;
fi

echo Driver tool test fasta_number is finished