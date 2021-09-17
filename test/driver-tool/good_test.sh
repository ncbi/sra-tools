#!/bin/bash

tool=$1
bin_dir=$2

echo "testing expected output for dry run of" ${tool}

TEMPDIR=.

mkdir -p actual

output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
PATH="${bin_dir}:$PATH" \
SRATOOLS_TESTING=2 \
SRATOOLS_IMPERSONATE=${tool} \
${bin_dir}/sratools SRR000001 ERR000001 DRR000001 2>actual/${tool}.stderr && \
diff expected/${tool}.stderr actual/${tool}.stderr)

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test ${tool} FAILED, res=$res output=$output" && exit 1;
fi

echo Driver tool test ${tool} is finished

