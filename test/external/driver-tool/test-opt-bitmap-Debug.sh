#!/bin/sh

bin_dir=$1
sratools=$2

TEST_DESC="setting command line options bits via ${sratools}"
echo "testing ${TEST_DESC}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=3 \
	SRATOOLS_IMPERSONATE=vdb-dump \
	${sratools} SRR053325 -CY -a -+KNS \
               2>&1 | grep VDB_OPT_BITMAP > actual/bitmap.stderr ; \
	diff expected/bitmap.stderr actual/bitmap.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test ${TEST_DESC} FAILED: res=$res output=<$output>"\
             && exit 1;
fi
rm -rf actual/bitmap.stderr

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=3 \
	SRATOOLS_IMPERSONATE=vdb-dump \
	${sratools} SRR053325 -CY -+KNS -a \
               2>&1 | grep VDB_OPT_BITMAP > actual/bitmap.stderr ; \
	diff expected/bitmap.stderr actual/bitmap.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test ${TEST_DESC} FAILED: res=$res output=<$output>"\
             && exit 1;
fi
rm -rf actual/bitmap.stderr

echo "Driver tool test ${TEST_DESC} succeed"
