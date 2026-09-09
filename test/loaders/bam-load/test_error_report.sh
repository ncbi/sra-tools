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

test -f "CM000682.1" || curl --silent --output "CM000682.1" "$("${DIRTOTEST}/srapath" "CM000682.1")"

mkdir -p actual

"${DIRTOTEST}/bam-load" \
    --errorReport actual/report.json.raw \
    --output actual/out.sra \
    --config "${CASE}/analysis.bam.cfg" \
    --log-level err \
    "${CASE}/${CASE}.sam" || { echo "bam-load ${CASE} failed"; exit 1; }

# remove what we expect to vary
jq 'del(.validator.version, .generatedAt)' actual/report.json.raw > actual/report.json

${DIFF} actual/report.json "expected/${CASE}.json" || \
    { echo "the files actual/report.json and expected/${CASE}.json differ!"; exit 1; }

rm -rf actual
test -f "CM000682.1" && rm -f "CM000682.1"
