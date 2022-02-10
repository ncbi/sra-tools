#!/bin/bash

container=$1
bin_dir=$2

echo "testing expected output for container" ${container}

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
PATH="${bin_dir}:$PATH" \
SRATOOLS_IMPERSONATE=fastq-dump \
${bin_dir}/sratools ${container} 2>actual/${container}.stderr ; \
diff expected/${container}.stderr actual/${container}.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test container ${container} FAILED, res=$res output=$output" && exit 1;
fi

echo Driver tool test container ${container} is finished