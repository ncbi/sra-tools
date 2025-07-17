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

# $1 - directory containing the binaries
# $2 - the executable to test
# $3 - test case ID
#
# return codes:
# 0 - passed
# 1 - coud not find the tool
# 2 -
# 3 - the dumper failed on the output of the loader
# 4 - outputs differ
bin_dir=$1
read_filter_redact=$2
TEST_CASE_ID=$3

DIFF="diff -b"
if [ "$(uname -s)" = "Linux" ] ; then
    if [ "$(uname -o)" = "GNU/Linux" ] ; then
        DIFF="diff -b -Z"
    fi
fi

echo Testing ${read_filter_redact} from ${bin_dir}

if ! test -f ${bin_dir}/${read_filter_redact}; then
    echo "${bin_dir}/${read_filter_redact} does not exist."
    exit 1
fi

RUN=./input/${TEST_CASE_ID}
FLT=./input/${TEST_CASE_ID}.in

# remove old test files
${bin_dir}/vdb-unlock actual/${TEST_CASE_ID} 2>/dev/null
rm -fr actual/${TEST_CASE_ID}

# prepare sources
mkdir -p actual # else kar will fail
${bin_dir}/kar --extract ${RUN} --directory actual/${TEST_CASE_ID}

# read-filter-redact
${bin_dir}/${read_filter_redact} -F${FLT} actual/${TEST_CASE_ID} #> /dev/null 2>&1

# verify the HISTORY metadata
${bin_dir}/kdbmeta actual/${TEST_CASE_ID} -TSEQUENCE HISTORY | sed 's/build=".*"//' >actual/${TEST_CASE_ID}.meta

${DIFF} actual/${TEST_CASE_ID}.meta expected/${TEST_CASE_ID}.meta
rc="$?"
if [ "$rc" != "0" ] ; then
    cat $TEMPDIR/diff
    echo "command executed:"
    echo $CMD
    exit 3
fi

# remove old test files
${bin_dir}/vdb-unlock actual/${TEST_CASE_ID} 2>/dev/null
rm -fr actual/${TEST_CASE_ID}
