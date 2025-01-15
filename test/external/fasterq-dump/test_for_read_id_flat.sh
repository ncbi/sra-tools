#!/usr/bin/env bash

# ================================================================
#
#   Test #1 : flat table
#
#   to keep the test quick we use a slice of ERR3487613
#   we are using ERR3487613.short - which has the first 10
#   spots of the original accession
#
# ================================================================

set -e

BINDIR="$1"

FASTERQDUMP="${BINDIR}/fasterq-dump"
[ -x $FASTERQDUMP ] || { echo "${FASTERQDUMP} not found - exiting..."; exit 3; }

FLAT_TABLE_ACC="ERR3487613"

[ -f $FLAT_TABLE_ACC ] || {
    echo "${FLAT_TABLE_ACC} not found here - recreating it"

    VDB_COPY="${BINDIR}/vdb-copy"
    [ -x "${VDB_COPY}" ] || { echo "${VDB_COPY} not found - exiting..."; exit 3; }

    KAR="${BINDIR}/kar"
    [ -x "${KAR}" ] || { echo "${KAR} not found - exiting..."; exit 3; }

    FLAT_TABLE_SHORT_DIR="${FLAT_TABLE_ACC}.dir"
    CMD="${VDB_COPY} ${FLAT_TABLE_ACC} ${FLAT_TABLE_SHORT_DIR} -R 1-10"
    eval "${CMD}"

    CMD="${KAR} -c ${FLAT_TABLE_ACC} -d ${FLAT_TABLE_SHORT_DIR}"
    eval "${CMD}"

    rm -rf "${FLAT_TABLE_SHORT_DIR}"
}

FLAT_TABLE_FASTA="${FLAT_TABLE_ACC}.fasta"
#run fasterq-dump on the short slice and produce unsorted FASTA
CMD="${FASTERQDUMP} ./${FLAT_TABLE_ACC} --fasta-unsorted -f"
eval "${CMD}"

READ_ID1=$(awk 'BEGIN{N=0} /^>/ && $1~/\/1$/ {N+=1} END{print N}' "${FLAT_TABLE_FASTA}")
READ_ID2=$(awk 'BEGIN{N=0} /^>/ && $1~/\/2$/ {N+=1} END{print N}' "${FLAT_TABLE_FASTA}")

[ $READ_ID1 -eq 10 ] || {
    echo "error number of '/1' reads should be 10, but it is ${READ_ID1}"
    exit 1
}
echo "READ_ID1: ${READ_ID1}"

[ $READ_ID2 -eq 10 ] || {
    echo "error number of '/2' reads should be 10, but it is ${READ_ID2}"
    exit 1
}
echo "READ_ID2: ${READ_ID2}"

# rm -rf "${FLAT_TABLE_FASTA}"
echo "success testing read-id for --fasta-unsorted mode on flat table"

