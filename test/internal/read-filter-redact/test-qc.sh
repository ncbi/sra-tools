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
# $3 - test case ID
# $4 - original run has no QC meta (optional)

bin_dir=$1
read_filter_redact=$2
TEST_CASE_ID=$3
NO_LOADED_QC=$4

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

RUN=./input/${TEST_CASE_ID}.sra
FLT=./input/Test_Read_filter_redact_1.in

# remove old test files
${bin_dir}/vdb-unlock actual/${TEST_CASE_ID} 2>/dev/null
rm -fr actual/${TEST_CASE_ID}

# prepare sources
mkdir -p actual # else kar will fail
${bin_dir}/kar --extract ${RUN} --directory actual/${TEST_CASE_ID} || exit 3
# lock the run to verify that read-filter-redact can unlock it
${bin_dir}/vdb-lock actual/${TEST_CASE_ID} || exit 3

# check loaded QC meta
if [ "$NO_LOADED_QC" = "" ] ; then
    ${bin_dir}/kdbmeta -u actual/${TEST_CASE_ID} -TSEQUENCE QC \
                                 > actual/${TEST_CASE_ID}.orig.all || exit 4
    grep -v '<timestamp>' actual/${TEST_CASE_ID}.orig.all \
                                     > actual/${TEST_CASE_ID}.orig || exit 5
    ${DIFF} actual/${TEST_CASE_ID}.orig expected/${TEST_CASE_ID}.orig
    rc="$?"
    if [ "$rc" != "0" ] ; then
        exit 6
    fi
    T_LOADED=$(xmllint --xpath /QC/current/timestamp \
        actual/${TEST_CASE_ID}.orig.all)
else
    ${bin_dir}/kdbmeta actual/${TEST_CASE_ID} -TSEQUENCE QC 2>&1 \
      | grep -q "failed to open node 'QC'"
    rc="$?"
    if [ "$rc" != "0" ] ; then
        echo QC found in load meta
        exit 7
    fi
fi

${bin_dir}/vdb-dump -CREAD,READ_FILTER actual/${TEST_CASE_ID} \
    > actual/${TEST_CASE_ID}.dump.orig || exit 8
${DIFF} actual/${TEST_CASE_ID}.dump.orig expected/${TEST_CASE_ID}.dump.orig
rc="$?"
if [ "$rc" != "0" ] ; then
    exit 9
fi

# read-filter-redact just READ_FILTER
${bin_dir}/${read_filter_redact} -F${FLT} actual/${TEST_CASE_ID} \
                                                  > /dev/null 2>&1 || exit 10

# QC did not change
if [ "$NO_LOADED_QC" = "" ] ; then
    ${bin_dir}/kdbmeta -u actual/${TEST_CASE_ID} -TSEQUENCE QC \
                                      > actual/${TEST_CASE_ID}.all || exit 11
    ${DIFF} actual/${TEST_CASE_ID}.all actual/${TEST_CASE_ID}.orig.all
    rc="$?"
    if [ "$rc" != "0" ] ; then
        exit 12
    fi
else
    ${bin_dir}/kdbmeta actual/${TEST_CASE_ID} -TSEQUENCE QC 2>&1 \
      | grep -q "failed to open node 'QC'"
    rc="$?"
    if [ "$rc" != "0" ] ; then
        echo QC found in load meta after read_filter_redact
        exit 13
    fi
fi

${bin_dir}/vdb-dump -CREAD,READ_FILTER actual/${TEST_CASE_ID} \
    > actual/${TEST_CASE_ID}.dump.flt || exit 14
${DIFF} actual/${TEST_CASE_ID}.dump.flt expected/${TEST_CASE_ID}.dump.flt
rc="$?"
if [ "$rc" != "0" ] ; then
    exit 15
fi

# read-filter-redact READ_FILTER and READ
${bin_dir}/${read_filter_redact} -F${FLT} actual/${TEST_CASE_ID} -r \
                                                  > /dev/null 2>&1 || exit 16

${bin_dir}/vdb-dump -CREAD,READ_FILTER actual/${TEST_CASE_ID} \
    > actual/${TEST_CASE_ID}.dump.read || exit 17
${DIFF} actual/${TEST_CASE_ID}.dump.read expected/${TEST_CASE_ID}.dump.read
rc="$?"
if [ "$rc" != "0" ] ; then
    exit 18
fi

# check QC meta after redaction
${bin_dir}/kdbmeta -u actual/${TEST_CASE_ID} -TSEQUENCE QC \
                                   > actual/${TEST_CASE_ID}.QC.all || exit 19
grep -v '<timestamp>' actual/${TEST_CASE_ID}.QC.all \
                                     > actual/${TEST_CASE_ID}.QC   || exit 20
${DIFF} actual/${TEST_CASE_ID}.QC expected/${TEST_CASE_ID}.QC
rc="$?"
if [ "$rc" != "0" ] ; then
    exit 21
fi

# compate original history meta with loaded
if [ "$NO_LOADED_QC" = "" ] ; then
   LOADED=$(xmllint --noblanks --xpath /QC/current \
       actual/${TEST_CASE_ID}.orig.all | sed 's/ //g')
   ORIGINAL=$(xmllint --noblanks --xpath /QC/history/event_1/original \
     actual/${TEST_CASE_ID}.QC.all | sed 's/original>/current>/' | sed 's/ //g')
   if [ "$LOADED" != "$ORIGINAL" ] ; then
       echo "/QC/history/event_1/original = '$ORIGINAL''"
       exit 22
   fi
fi

# compate timestamps
F=actual/${TEST_CASE_ID}.QC.all
CURRENT=$(xmllint --xpath /QC/current/timestamp           $F)
CMP=$(xmllint --xpath /QC/history/event_1/added/timestamp $F)
if [ "$CURRENT" != "$CMP" ] ; then
    echo "current/timestamp($CURRENT) != added/timestamp($CMP)"
    exit 23
fi
CMP=$(xmllint --xpath /QC/history/event_1/removed/timestamp $F)
if [ "$CURRENT" != "$CMP" ] ; then
    echo "current/timestamp($CURRENT) != removed/timestamp($CMP)"
    exit 24
fi
if [ "$NO_LOADED_QC" = "" ] ; then
# current/timestamp needs to change
  CMP=$(xmllint --xpath /QC/current/timestamp $F)
  if [ "$T_LOADED" = "$CMP" ] ; then
    echo "loaded timestamp($T_LOADED) == QC/current/timestamp($CMP)"
    exit 25
  fi
else
# no timestamp in load; it was generated by read_filter_redact
  CMP=$(xmllint --xpath /QC/current/timestamp $F)
  if [ "$CURRENT" != "$CMP" ] ; then
    echo "current/timestamp($CURRENT) != QC/current/timestamp($CMP)"
    exit 26
  fi
  CMP=$(xmllint --xpath /QC/history/event_1/original/timestamp $F)
  if [ "$CURRENT" != "$CMP" ] ; then
    echo "current/timestamp($CURRENT) != original/timestamp($CMP)"
    exit 27
  fi
fi

# remove old test files
${bin_dir}/vdb-unlock actual/${TEST_CASE_ID}
rm -fr actual/${TEST_CASE_ID}
