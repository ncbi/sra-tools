#!/bin/bash

# SRATOOLS_TESTING=5 and skip SDL via config, sub-tool invocation is
# simulated to always succeed, but everything up to the exec call is real

bin_dir=$1
sratools=$2

echo "testing expected output for dry run with no SDL via ${sratools}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp2.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=5 \
	SRATOOLS_IMPERSONATE=fastq-dump \
	${bin_dir}/${sratools} SRR000001 2>actual/NO_SDL.stderr && \
	diff expected/NO_SDL.stderr actual/NO_SDL.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test NO_SDL via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/NO_SDL.stderr

echo Driver tool test NO_SDL via ${sratools} is finished
