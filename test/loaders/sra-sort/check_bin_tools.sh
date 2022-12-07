#!/usr/bin/env bash

# common helper script to check for existence and executability of binary tools
# to be called by other scripts...
#

BINDIR="$1"
VERBOSE="$2"
KAR="${BINDIR}/kar"
BAMLOAD="${BINDIR}/bam-load"
SRASORT="${BINDIR}/sra-sort"
SRASTAT="${BINDIR}/sra-stat"
SAMFACTORY="${BINDIR}/sam-factory"

#------------------------------------------------------------
# function to print message if $VERBOSE is not empty

#------------------------------------------------------------
# let us check if the tools we depend on do exist
function print_verbose {
    if [ -n "$VERBOSE" ]; then
        echo "$1"
    fi
}

for TOOL in $KAR $BAMLOAD $SRASORT $SRASTAT $SAMFACTORY
do
    if [[ ! -x "$TOOL" ]]; then
        echo "$TOOL - executable not found"
        exit 3
    fi
done

print_verbose "kar, bam-load, sra-sort, sra-stat and sam-factory executables found!"
