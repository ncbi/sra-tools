#!/bin/bash

bin_dir=$1

echo "testing expected output for run with vdbcache"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=5 \
	SRATOOLS_IMPERSONATE=vdb-dump \
	${bin_dir}/sratools SRR390728 2>actual/vdbcache.stderr ; \
	diff expected/vdbcache.stderr actual/vdbcache.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test vdbcache FAILED, res=$res output=$output" && exit 1;
fi

echo Driver tool test vdbcache is finished