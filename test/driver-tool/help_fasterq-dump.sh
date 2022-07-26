#!/bin/bash

bin_dir=$1
sratools=$2

echo "testing expected output for fasterq-dump --help via ${sratools}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_IMPERSONATE=fasterq-dump \
	${bin_dir}/${sratools} --help | sed -e'/"fasterq-dump" version/ s/version.*/version <deleted>/' >actual/help_fasterq-dump.stdout ; \
	diff expected/help_fasterq-dump.stdout actual/help_fasterq-dump.stdout)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test help_fasterq-dump via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/help_fasterq-dump.stdout

echo Driver tool test help_fasterq-dump via ${sratools} is finished