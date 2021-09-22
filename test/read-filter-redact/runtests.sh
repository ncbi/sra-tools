#!/bin/bash

bin_dir=$1

echo Testing read-filter-redact from ${bin_dir}


# rm -rf actual
# mkdir -p actual
# NCBI_SETTINGS=/ ${bin_dir}/sra-stat -x SRR053325 > actual/SRR053325
# output=$(diff actual/SRR053325 expected/SRR053325-biological-reloaded)
# res=$?
# if [ "$res" != "0" ];
	# then echo "quick_bases test FAILED, res=$res output=$output" && exit 1;
# fi


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
${bin_dir}/read-filter-redact -F${FLT} ${RUN} > /dev/null 2>&1

# make sure HISTORY meta was created
${bin_dir}/kdbmeta tmp-read-filter-redact-test-run -TSEQUENCE HISTORY | \
	grep '^  <EVENT_1 build="' | grep '" run="' | \
	grep '" tool="read-filter-redact" vers="' > /dev/null

# remove old test files
${bin_dir}/vdb-unlock ${RUN}
rm -fr tmp-read-filter-redact-test-*
