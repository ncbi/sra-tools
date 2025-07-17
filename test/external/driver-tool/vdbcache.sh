#!/bin/sh

bin_dir=$1
sratools=$2

echo "testing expected output for run with vdbcache via ${sratools}"

TEMPDIR=.

mkdir -p actual

echo ${bin_dir}/${sratools} SRR341578

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=5 \
	SRATOOLS_IMPERSONATE=vdb-dump \
	${bin_dir}/${sratools} SRR341578 2>actual/vdbcache.stderr ; \
	diff expected/vdbcache.stderr actual/vdbcache.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test vdbcache via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/vdbcache.stderr

echo Driver tool test vdbcache via ${sratools} is finished
