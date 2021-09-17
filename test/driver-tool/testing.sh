#!/bin/bash

bin_dir=$1

echo "testing expected output for testing modes with bad inputs testing"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=prefetch \
	${bin_dir}/sratools --perm foo.jwt --cart foo.cart --ngc foo.ngc DRX000001 2>actual/testing.stderr ; \
	diff expected/testing.stderr actual/testing.stderr || diff expected/testing-cloudy.stderr actual/testing.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test testing FAILED, res=$res output=$output" && exit 1;
fi

echo Driver tool test testing is finished