#!/bin/bash

echo $*

BINDIR=$1
PYTHON=$2
WORKDIR=$3
OUTPUT=$4

echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' > ${WORKDIR}/tmp.mkfg
echo '/sra/quality_type = "raw_scores"' >>${WORKDIR}/tmp.mkfg

${PYTHON} generate-test-data.py ${WORKDIR}/test-data || exit 1
${BINDIR}/kar -d ${WORKDIR}/test-data -fc ${OUTPUT} || exit 2
rm -rf ${WORKDIR}/test-data expected