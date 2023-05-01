#!/bin/bash

tool=$1
bin_dir=$2
sratools=$3

SRR000001="https://sra-download-internal.ncbi.nlm.nih.gov/sos5/sra-pub-zq-11/SRR000/000/SRR000001/SRR000001.lite.1"
ERR000001="https://sra-download-internal.ncbi.nlm.nih.gov/sos5/sra-pub-zq-11/ERR000/000/ERR000001/ERR000001.sralite.1"
DRR000001="https://sra-download-internal.ncbi.nlm.nih.gov/sos5/sra-pub-zq-11/DRR000/001/DRR000001.sralite.1"

echo "testing expected output for dry run of ${tool} via ${sratools} (url)"

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
        PATH="${bin_dir}:$PATH" \
        SRATOOLS_TESTING=2 \
        SRATOOLS_IMPERSONATE=${tool} \
        ${bin_dir}/${sratools} ${SRR000001} ${ERR000001} ${DRR000001} 2>actual/${tool}_url.stderr && \
        diff expected/${tool}_url.stderr actual/${tool}_url.stderr)

res=$?
if [ "$res" != "0" ];
	then cat actual/${tool}_url.stderr && echo "Driver tool test ${tool}_url via ${sratools} FAILED, res=$res output=$output" && exit 1;
fi
rm -rf actual/${tool}_url.stderr

echo "Driver tool test ${tool} via ${sratools} is finished (url)"

