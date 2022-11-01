#!/bin/sh

BINDIR="${1}"
VERSION="${2}"
shift 2

ALL_INSTALLED='YES'
ANY_INSTALLED='NO'
for exe in 'vdb-dump' 'fasterq-dump' 'sam-dump' 'sra-pileup'
do
    if [ -x "${BINDIR}/${exe}-orig.${VERSION}" ]
    then
        ANY_INSTALLED='YES'
    else
        ALL_INSTALLED='NO'
    fi
done

if [ "${ALL_INSTALLED}" != "${ANY_INSTALLED}" ]
then
    echo 'some tools are installed and some are not!?' >&2
    for exe in 'vdb-dump' 'fasterq-dump' 'sam-dump' 'sra-pileup'
    do
        if [ -x "${BINDIR}/${exe}-orig.${VERSION}" ]
        then
            echo "found installed ${exe}"
        else
            echo "did not find installed ${exe}"
        fi
    done
    exit 1
fi
INSTALLED=${ALL_INSTALLED}

TEST_SOURCE="${PWD}"
"${BINDIR}/sratools.${VERSION}" print-args-json \
| python3 "${TEST_SOURCE}/test-tool-args.py" --installed "${INSTALLED}" --path "${BINDIR}" --use-version "${VERSION}" "${@}" \
| sh
