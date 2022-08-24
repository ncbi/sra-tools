#!/usr/bin/env bash

# common helper script to check for existence and executability of binary tools
# to be called by other scripts...
#

BINDIR="$1"
SAMDUMP="${BINDIR}/sam-dump"
BAMLOAD="${BINDIR}/bam-load"
KAR="${BINDIR}/kar"

#------------------------------------------------------------
# let us check if the tools we depend on do exist
if [[ ! -x "$SAMDUMP" ]]; then
    echo "$SAMDUMP - executable not found"
	exit 3
fi

if [[ ! -x "$BAMLOAD" ]]; then
    echo "$BAMLOAD - executable not found"
	exit 3
fi

if [[ ! -x "$KAR" ]]; then
    echo "$KAR - executable not found"
	exit 3
fi

echo "sam-dump, bam-load, and kar executables found!"
