#!/bin/sh

bin_dir=$1
read_filter_redact=$2
TEST_CASE_ID=$3

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
echo 1 > ${FLT}
${bin_dir}/kar --extract ${RUN} --directory actual/${TEST_CASE_ID}

# make sure HISTORY meta does not exist
if ${bin_dir}/kdbmeta ${RUN}/SEQUENCE HISTORY 2>/dev/null; \
	then echo "error: HISTORY found in source metadata"; exit 2; fi

# read-filter-redact
${bin_dir}/${read_filter_redact} -F${FLT} actual/${TEST_CASE_ID} #> /dev/null 2>&1

# make sure HISTORY meta was created
${bin_dir}/kdbmeta actual/${TEST_CASE_ID} -TSEQUENCE HISTORY | \
	grep '^  <EVENT_1 build="' | \
	grep '" run="' | \
	grep '" tool="read-filter-redact" vers="' | \
	grep 'fingerprint' 2>actual/${TEST_CASE_ID}.grep
res=$?
if [ "$res" != "0" ];
	then echo "metadata check FAILED, res=$res" && exit 3;
fi

# remove old test files
${bin_dir}/vdb-unlock actual/${TEST_CASE_ID} 2>/dev/null
rm -fr actual/${TEST_CASE_ID}
