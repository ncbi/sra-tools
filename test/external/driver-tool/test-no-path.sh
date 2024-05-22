#!/bin/sh

bin_dir=$1
sratools=$2

TEST_DESC="path resolution via full path to ${sratools}"
echo "testing ${TEST_DESC}"

TEMPDIR=.

mkdir -p actual

# verify that driver tool locates target executable via full path
output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	SRATOOLS_TESTING=6 \
	SRATOOLS_IMPERSONATE=prefetch \
	"${bin_dir}/${sratools}" 2>actual/no-path.stderr ; \
	grep "executable-path" actual/no-path.stderr | \
	grep -q "${bin_dir}")

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test ${TEST_DESC}, res=$res output=$output" && exit 1;
fi
rm -rf actual/no-path.stderr

if [ -f "${TEMPDIR}/prefetch" ];
	then echo "Driver tool test ${TEST_DESC}, skipping part 2"
fi

echo "Driver tool test ${TEST_DESC} is finished"
