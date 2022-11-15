#!/bin/bash

bin_dir=$1
sratools=$2

echo "testing expected output for srapath --help via ${sratools}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_IMPERSONATE=srapath \
	${bin_dir}/${sratools} --help | sed -e'/"srapath" version/ s/version.*/version <deleted>/' >actual/help_srapath.stdout ; \
	diff expected/help_srapath.stdout actual/help_srapath.stdout)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test help_srapath via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/help_srapath.stdout

echo Driver tool test help_srapath via ${sratools} is finished