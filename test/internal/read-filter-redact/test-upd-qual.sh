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

# $1 - directory containing the binaries
# $2 - the executable to test

bin_dir=$1
read_filter_redact=$2
TEST_CASE_ID=upd-qual

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

if ! test -f ${bin_dir}/vdb-unlock; then
    echo "${bin_dir}/vdb-unlock does not exist."
    exit 2
fi

RUN=../../../test/external/sra-info/input/MultiPlatform.sra
FLT=./input/Test_Read_filter_redact_1.in

# remove old test files
${bin_dir}/vdb-unlock actual/${TEST_CASE_ID} 2>/dev/null
rm -fr actual/${TEST_CASE_ID}

# prepare sources
mkdir -p actual # else kar will fail
#echo ${bin_dir}/kar --extract ${RUN} --directory actual/${TEST_CASE_ID}
${bin_dir}/kar --extract ${RUN} --directory actual/${TEST_CASE_ID} || exit 3

# lock the run to verify that read-filter-redact can unlock it
#echo ${bin_dir}/vdb-lock actual/${TEST_CASE_ID}
${bin_dir}/vdb-lock actual/${TEST_CASE_ID} || exit 3

${bin_dir}/vdb-dump -CQUALITY,READ,READ_FILTER actual/${TEST_CASE_ID} \
    > actual/${TEST_CASE_ID}.dump.orig || exit 8
${DIFF} actual/${TEST_CASE_ID}.dump.orig expected/${TEST_CASE_ID}.dump.orig
rc="$?"
if [ "$rc" != "0" ] ; then
    exit 9
fi

# read-filter-redact just READ_FILTER
${bin_dir}/${read_filter_redact} -F${FLT} actual/${TEST_CASE_ID} \
                                                  > /dev/null 2>&1 || exit 10

${bin_dir}/vdb-dump -CQUALITY,READ,READ_FILTER actual/${TEST_CASE_ID} \
    > actual/${TEST_CASE_ID}.dump.flt || exit 14
${DIFF} actual/${TEST_CASE_ID}.dump.flt expected/${TEST_CASE_ID}.dump.flt
rc="$?"
if [ "$rc" != "0" ] ; then
    exit 15
fi

# read-filter-redact READ_FILTER and READ/QUALITY
${bin_dir}/${read_filter_redact} -F${FLT} actual/${TEST_CASE_ID} -r \
                                                  > /dev/null 2>&1 || exit 16

${bin_dir}/vdb-dump -CQUALITY,READ,READ_FILTER actual/${TEST_CASE_ID} \
    > actual/${TEST_CASE_ID}.dump.read || exit 17
${DIFF} actual/${TEST_CASE_ID}.dump.read expected/${TEST_CASE_ID}.dump.read
rc="$?"
if [ "$rc" != "0" ] ; then
    exit 18
fi

# remove old test files
${bin_dir}/vdb-unlock actual/${TEST_CASE_ID}
rm -fr actual/${TEST_CASE_ID}
