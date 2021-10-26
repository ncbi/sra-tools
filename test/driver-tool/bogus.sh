#!/bin/bash

bin_dir=$1

echo "testing expected output for unknown tool"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_IMPERSONATE=rcexplain \
	${bin_dir}/sratools 2>actual/bogus.stderr ; \
	diff expected/bogus.stderr actual/bogus.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test unknown tool, res=$res output=$output" && exit 1;
fi

echo Driver tool test unknown tool is finished