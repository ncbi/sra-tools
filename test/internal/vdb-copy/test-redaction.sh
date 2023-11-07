#!/bin/bash

SCRIPT_PATH=$(dirname "${0}")
SCRIPT_PATH=$(cd "${SCRIPT_PATH}"; pwd)

CONFIG_PATH="${SCRIPT_PATH}/../../../tools/internal/vdb-copy"
CONFIG_PATH=$(cd "${CONFIG_PATH}"; pwd)

TOOL_PATH="${1}"
if [ ! -d "${TOOL_PATH}" ]; then
    TOOL_PATH=$(dirname "${TOOL_PATH}")
fi
TOOL_PATH=$(cd "${TOOL_PATH}"; pwd)

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

SCRATCH=$(mktemp -d) || exit 1
(
    cd "${SCRIPT_PATH}/redact-test"
    python3 'generate-test-data.py' "${SCRATCH}/test-data" || exit $?
    cd ${SCRATCH} || exit 1
    "${VDB_COPY}" -k "${CONFIG_PATH}" 'test-data' 'test-data-redacted' || exit $?
    "${VDB_DUMP}" -f tab 'test-data-redacted' -C"READ_FILTER,(INSDC:dna:text)READ" | \
        awk 'BEGIN{ FS="\t" } $1~/REDACTED/ && $2~/[^N]/ {exit 3}'
)
ec=$?
rm -rf "${SCRATCH}"
exit $ec
