#!/bin/sh

container=$1
bin_dir=$2
sratools=$3

echo "testing expected output for container ${container} via ${sratools}"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
         PATH="${bin_dir}:$PATH" \
         SRATOOLS_IMPERSONATE=fastq-dump \
         ${bin_dir}/${sratools} ${container} 2>actual/${container}.stderr ; \
         diff expected/${container}.stderr actual/${container}.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test container ${container} via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/${container}.stderr

echo Driver tool test container ${container} via ${sratools} is finished
