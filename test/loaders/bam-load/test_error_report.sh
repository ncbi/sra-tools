#!/bin/sh
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

DIRTOTEST="${1}"
CASE="${2}"

DIFF="diff -b"
if [ "$(uname -s)" = "Linux" ] ; then
    if [ "$(uname -o)" = "GNU/Linux" ] ; then
        DIFF="diff -b -Z"
    fi
fi

HEADER=""
SAM=""
# Search the test case directory for header and sam files. Not all
# test cases have a header file.
{
    WD=$(pwd)
    cd "${CASE}" || exit 1
    if [ -f "${CASE}.header.sam" ] ; then
        HEADER="--header ${CASE}/${CASE}.header.sam"
    fi
    for s in $(ls ${CASE}*.sam | sort -d) ; do
        if [ "${s}" != "${CASE}.header.sam" ] ; then
            SAM="${SAM} ${CASE}/${s}"
        fi
    done
    cd "${WD}"
}
# make $@ equal to the specify arguments for bam-load for this
# test case, i.e. the header (if any) + the sam files in the test
# case directory.
set -- "${HEADER}" ${SAM}

# Because configuration has been neutered, bam-load will not find
# the reference if it is remote. So, check with srapath and fetch
# it first if it is a remote path.
REF="$("${DIRTOTEST}/srapath" "CM000682.1")"
test -f "${REF}" || curl --silent --output "CM000682.1" "${REF}"

mkdir -p actual

"${DIRTOTEST}/bam-load" \
    --errorReport actual/report.raw.json \
    --output actual/out.sra \
    --config analysis.bam.cfg \
    --log-level err \
    $@ || { echo "bam-load ${CASE} failed"; exit 1; }

# remove what we expect to vary
jq 'del(.validator.version, .generatedAt)' actual/report.raw.json > actual/report.json

${DIFF} actual/report.json "expected/${CASE}.json" || \
    { echo "the files actual/report.json and expected/${CASE}.json differ!"; exit 1; }

rm -rf actual
rm -f "CM000682.1"
