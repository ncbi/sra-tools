#!/bin/bash

bin_dir=$1

echo "testing expected output for prefetch --help"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_IMPERSONATE=prefetch \
	${bin_dir}/sratools --help | sed -e'/"prefetch" version/ s/version.*/version <deleted>/' >actual/help_prefetch.stdout ; \
	diff expected/help_prefetch.stdout actual/help_prefetch.stdout)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test help_prefetch FAILED, res=$res output=$output" && exit 1;
fi

echo Driver tool test help_prefetch is finished