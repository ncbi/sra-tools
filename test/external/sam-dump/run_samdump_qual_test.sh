#!/usr/bin/env bash

set -e

TOOL="sam-dump"
TOOLFIND=`which $TOOL`
TOOLPATH=`dirname $TOOLFIND`

if [[ -z "$TOOLPATH" ]]; then
	ARCH=`arch`
	TOOLPATH="$HOME/ncbi-outdir/sra-tools/linux/gcc/$ARCH/dbg/bin"
fi

if [[ ! -f "${TOOLPATH}/${TOOL}" ]]; then
	echo "cannot find tool to test... >${TOOLPATH}/${TOOL}<"
	exit 3
else
	echo "found sam-dump at: ${TOOLPATH}"
fi

#small cSRA accession
ACC="SRR2055116"
./verify_omit_qual.sh $ACC $TOOLPATH

#small table
ACC="SRR8048975"
./verify_omit_qual.sh $ACC $TOOLPATH

#small none-cSRA accession
ACC="SRR9064703"
./verify_omit_qual.sh $ACC $TOOLPATH

echo -e "\nsuccess!"
