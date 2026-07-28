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
#echo "$0 $*"

# $1 - command line for the tool
# $2 - work directory (expected results under expected/, actual results and temporaries created under actual/)
#
# return codes:
# 0 - passed
# 1 - coud not create temp dir
# 2 - non-0 return code from the tool
# 3 - outputs differ

TOOL=$1
RC=0
THIS="kdbmeta-tests"

EXE="${TOOL%% *}"
if ! test -x "${EXE}"; then
    echo "${EXE} does not exist. Skipping the test."
    exit 0
fi

DIFF="diff -b"
if [ "$(uname -s)" = "Linux" ] ; then
    if [ "$(uname -o)" = "GNU/Linux" ] ; then
        DIFF="diff -b -Z"
    fi
fi

TOP_WD="${PWD}"
save_traps=$(trap)
trap exit INT HUP TERM

TEMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/${THIS}.XXXXXX") || { echo "Can't make a temp directory!"; exit $?; }
cleanup () {
    ec=$?
    cd "${TOP_WD}"
    rm -rf "${TEMPDIR}"
    eval "${save_traps}"
    exit ${ec}
}
trap cleanup EXIT

cd "${TEMPDIR}" || { echo "Can't use temp directory!"; exit $?; }

cp -r "${TOP_WD}/../sra-info/input/Test_Sra_Info_Fingerprint" "db"

# set the value
"${TOOL}" db FOO=BAR
rc="${?}"
if [ "${rc}" != "${RC}" ] ; then
    echo "${TOOL} returned ${rc}, expected ${RC}"
    echo "command executed:"
    echo "${TOOL}" "db" "FOO=BAR"
    exit 2
fi

# get the value
BAR="$( "${TOOL}" db FOO )"
rc="${?}"
if [ "${rc}" != "${RC}" ] ; then
    echo "${TOOL} returned ${rc}, expected ${RC}"
    echo "command executed:"
    echo "${TOOL}" "db" "FOO"
    exit 2
fi

# check the value
if [ "${BAR}" != "<FOO>'BAR'</FOO>" ] ; then
    echo "unexpected result from ${TOOL}; got ${BAR}, expected <FOO>'BAR'</FOO>"
    echo "command executed:"
    echo "${TOOL}" "db" "FOO"
    exit 3
fi

# overwrite the value
"${TOOL}" db FOO=
rc="${?}"
if [ "${rc}" != "${RC}" ] ; then
    echo "${TOOL} returned ${rc}, expected ${RC}"
    echo "command executed:"
    echo "${TOOL}" "db" "FOO=BAR"
    exit 2
fi

# get the value
EMPTY="$( "${TOOL}" db FOO )"
rc="${?}"
if [ "${rc}" != "${RC}" ] ; then
    echo "${TOOL} returned ${rc}, expected ${RC}"
    echo "command executed:"
    echo "${TOOL}" "db" "FOO"
    exit 2
fi

# check the value
if [ "${EMPTY}" != "<FOO/>" ] ; then
    echo "unexpected result from ${TOOL}; got ${EMPTY}, expected <FOO/>"
    echo "command executed:"
    echo "${TOOL}" "db" "FOO"
    exit 3
fi

# delete just in case
"${TOOL}" db --delete A 2>/dev/null

ESC_SEQ='\x9B\x28\x03\x00\x00\x00\x00\x00'
ESC_SEQ2='\x30\x31\x32'

# set the a numeric node value using escape sequences
"${TOOL}" db A="${ESC_SEQ}"
rc="${?}"
if [ "${rc}" != "${RC}" ] ; then
    echo "${TOOL} returned ${rc}, expected ${RC}"
    echo "command executed:"
    echo "${TOOL}" "db" "A=${ESC_SEQ}"
    exit 2
fi

VALUE="$( "${TOOL}" db A )"
rc="${?}"
if [ "${rc}" != "${RC}" ] ; then
    echo "${TOOL} returned ${rc}, expected ${RC}"
    echo "command executed:"
    echo "${TOOL}" "db" "A"
    exit 2
fi

if [ "<A>${ESC_SEQ}</A>" != "${VALUE}" ] ; then
    echo "unexpected result from ${TOOL}; got ${VALUE}, expected <A>${ESC_SEQ}</A>"
    echo "command executed:"
    echo "${TOOL}" "db" "A"
    exit 3
fi

# try to set the a numeric attribute value with embedded nulls using escape sequences
"${TOOL}" db A/@a="${ESC_SEQ}" 2>/dev/null
rc="${?}"
if [ "${rc}" == "0" ] ; then
    echo "${TOOL} returned ${rc}, expected non-zero"
    echo "command executed:"
    echo "${TOOL}" "db" "A/@a=${ESC_SEQ}"
    exit 2
fi

# try to set the a numeric attribute value using escape sequences
"${TOOL}" db A/@a="${ESC_SEQ2}"
rc="${?}"
if [ "${rc}" != "${RC}" ] ; then
    echo "${TOOL} returned ${rc}, expected ${RC}"
    echo "command executed:"
    echo "${TOOL}" "db" "A/@a=${ESC_SEQ2}"
    exit 2
fi

VALUE="$( "${TOOL}" db A/@a )"
rc="${?}"
if [ "${rc}" != "${RC}" ] ; then
    echo "${TOOL} returned ${rc}, expected ${RC}"
    echo "command executed:"
    echo "${TOOL}" "db" "A/@a"
    exit 2
fi

if [ "<A a=\"012\"/>" != "${VALUE}" ] ; then
    echo "unexpected result from ${TOOL}; got ${VALUE}, expected <A a=\"012\"/>"
    echo "command executed:"
    echo "${TOOL}" "db" "A/@a"
    exit 3
fi
