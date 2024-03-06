#!/bin/sh

bin_dir=$1
read_filter_redact=$2

echo Testing ${read_filter_redact} from ${bin_dir}

if ! test -f ${bin_dir}/${read_filter_redact}; then
    echo "${bin_dir}/${read_filter_redact} does not exist. Skipping the test."
    exit 0
fi

RUN=tmp-read-filter-redact-test-run
FLT=tmp-read-filter-redact-test-in

# remove old test files
if ${bin_dir}/vdb-unlock ${RUN} 2>/dev/null ; then echo ; fi
rm -fr tmp-read-filter-redact-test-*


# prepare sources
echo 1 > ${FLT}
${bin_dir}/kar --extract ../align-cache/CSRA_file \
	--directory ${RUN}

# make sure HISTORY meta does not exist
if ${bin_dir}/kdbmeta tmp-read-filter-redact-test-run \
			-TSEQUENCE HISTORY 2>/dev/null; \
	then echo "error: HISTORY found in source metadata"; exit 1; fi

# read-filter-redact
${bin_dir}/${read_filter_redact} -F${FLT} ${RUN} > /dev/null 2>&1

# make sure HISTORY meta was created
${bin_dir}/kdbmeta tmp-read-filter-redact-test-run -TSEQUENCE HISTORY | \
	grep '^  <EVENT_1 build="' | grep '" run="' | \
	grep '" tool="read-filter-redact" vers="' > /dev/null

# remove old test files
${bin_dir}/vdb-unlock ${RUN}
rm -fr tmp-read-filter-redact-test-*
