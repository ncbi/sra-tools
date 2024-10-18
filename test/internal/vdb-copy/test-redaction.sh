#!/bin/sh

# test-redaction.sh <DIRTOTEST> <VDB_BINDIR> <VDB_INCDIR> <VDB_PATH>
# DIRTOTEST is the path where vdb-copy and vdb-dump are.
# VDB_BINDIR is the path where the ncbi-wvdb library is.
# VDB_INCDIR is the ':'-separated search path for the schema files
# VDB_PATH is the root of the ncbi-vdb source tree

SCRIPT_PATH=$(dirname "${0}")
SCRIPT_PATH=$(cd "${SCRIPT_PATH}"; pwd)

CONFIG_PATH="${SCRIPT_PATH}/../../../tools/internal/vdb-copy"
CONFIG_PATH=$(cd "${CONFIG_PATH}"; pwd)

TOOL_PATH="${1}"
if [ ! -d "${TOOL_PATH}" ]; then
    TOOL_PATH=$(dirname "${TOOL_PATH}")
fi
TOOL_PATH=$(cd "${TOOL_PATH}"; pwd)

VDB_LIBRARY_PATH="${2}"

VDB_INCLUDE_PATH="${3}"

VDB_PATH="${4}"

VDB_COPY="${TOOL_PATH}/vdb-copy"
if [ ! -x "${VDB_COPY}" ]; then
    echo "cannot find executable for vdb-copy"
    exit 3
fi
#echo "using ${VDB_COPY}"

VDB_DUMP="${TOOL_PATH}/vdb-dump"
if [ ! -x "${VDB_DUMP}" ]; then
    echo "cannot find executable for vdb-dump"
    exit 3
fi
#echo "using ${VDB_DUMP}"

which python3 && echo "python3 found" || echo "python3 not found: skipping the test"
which python3 || exit 0

SCRATCH=$(mktemp -d) || exit 1
(
    cd "${SCRIPT_PATH}/redact-test"
    env VDB_LIBRARY_PATH="${VDB_LIBRARY_PATH}" \
        VDB_PATH="${VDB_PATH}" \
        VDB_INCLUDE_PATH="${VDB_INCLUDE_PATH}" \
        python3 'generate-test-data.py' "${SCRATCH}/test-data" || \
        exit $?

    cd ${SCRATCH} || exit 1

    # Verify that redaction is needed. NB. this should exit 3 if awk does not exit 3.
    "${VDB_DUMP}" -f tab 'test-data' -C"READ_FILTER,(INSDC:dna:text)READ" | \
        awk 'BEGIN{ FS="\t" } $1~/REDACTED/ && $2~/[^N]/ {exit 3}' && exit 3;

    # Do the redaction.
    "${VDB_COPY}" -k "${CONFIG_PATH}" 'test-data' 'test-data-redacted' || exit $?

    # Verify that redaction worked. awk will exit 3 if any redacted spot's sequence has anything but N
    "${VDB_DUMP}" -f tab 'test-data-redacted' -C"READ_FILTER,(INSDC:dna:text)READ" | \
        awk 'BEGIN{ FS="\t" } $1~/REDACTED/ && $2~/[^N]/ {exit 3}'
)
ec=$?
rm -rf "${SCRATCH}"
exit $ec
