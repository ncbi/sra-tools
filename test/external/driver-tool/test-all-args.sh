#!/bin/sh

BINDIR="${1}"
VERSION="${2}"
shift 2

ALL_INSTALLED='YES'
ANY_INSTALLED='NO'
for exe in 'vdb-dump' 'fasterq-dump' 'sam-dump' 'sra-pileup'
do
    [ -x "${BINDIR}/${exe}-orig.${VERSION}" ] || ALL_INSTALLED='NO'
    [ -x "${BINDIR}/${exe}-orig.${VERSION}" ] && ANY_INSTALLED='YES'
done

[ "${ALL_INSTALLED}" == "${ANY_INSTALLED}" ] || { echo 'some tools are installed and some are not!?' >&2 ; exit 1; }
INSTALLED=${ALL_INSTALLED}

TEST_SOURCE="${PWD}"
"${BINDIR}/sratools.${VERSION}" print-args-json \
| python3 "${TEST_SOURCE}/test-tool-args.py" --installed "${INSTALLED}" --path "${BINDIR}" --use-version "${VERSION}" "${@}" \
| sh
