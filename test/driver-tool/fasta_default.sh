#!/bin/bash

bin_dir=$1

echo "testing expected output for fastq-dump --fasta default <run>"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	${bin_dir}/sratools --fasta default SRR390728 2>actual/fasta_default.stderr ; \
	diff expected/fasta_default.stderr actual/fasta_default.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test fasta_default FAILED, res=$res output=$output" && exit 1;
fi

echo Driver tool test fasta_default is finished