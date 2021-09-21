#!/bin/bash

vdb-dump-makedb=$1
bin_dir=$2
python_bin=$3
cmake_source_dir=$4

echo cmake_source_dir: ${cmake_source_dir}

echo "testing vdb_dump"

TEMPDIR=.
CONFIGTOUSE ?= NCBI_SETTINGS
TOP=${cmake_source_dir}
echo TOP=${TOP}

echo Testing ${bin_dir} CONFIGTOUSE=${CONFIGTOUSE}

# TODO testing_returncode

rm -rf actual
mkdir -p actual

if [ "${python_bin}" ]
	then echo "python_bin is ${python_bin}"
	${CONFIGTOUSE}=${TEMPDIR}/tmp.mkfg ${bin_dir}/vdb-dump SRR056386 -R 1 -C READ -f tab \
	    >actual/1.0.stdout && diff expected/1.0.stdout actual/1.0.stdout
	${CONFIGTOUSE}=/ ${bin_dir}/vdb-dump SRR056386 -R 1 -C READ -f tab -I \
	    >actual/1.1.stdout && diff expected/1.1.stdout actual/1.1.stdout
	# nested databases
	${CONFIGTOUSE}=/ ${bin_dir}/vdb-dump -E data/NestedDatabase \
	    >actual/2.0.stdout && diff expected/2.0.stdout actual/2.0.stdout
	${CONFIGTOUSE}=/ ${bin_dir}/vdb-dump -T SUBDB_1.SUBSUBDB_1.TABLE1 \
	       data/NestedDatabase >actual/2.1.stdout \
	    && diff expected/2.1.stdout actual/2.1.stdout
	${CONFIGTOUSE}=/ ${bin_dir}/vdb-dump -T SUBDB_1.SUBSUBDB_2.TABLE2 \
	       data/NestedDatabase >actual/2.2.stdout \
	    && diff expected/2.2.stdout actual/2.2.stdout
	rm -rf actual
	rm -rf data
	${python_bin} ${TOP}/build/check-exit-code.py ${bin_dir}/vdb-dump
	./test_buffer_insufficient.sh ${bin_dir}/vdb-dump VDB-3937.kar
	echo "...all tests passed"
else
	echo "python_bin is empty"
fi

#output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
#	PATH="${bin_dir}:$PATH" \
#	SRATOOLS_IMPERSONATE=fastq-dump \
#	${bin_dir}/sratools --help | sed -e'/"fastq-dump" version/ s/version.*/version <deleted>/' >actual/help_fastq-dump.stdout ; \
#	diff expected/help_fastq-dump.stdout actual/help_fastq-dump.stdout)


res=$?
if [ "$res" != "0" ];
	then echo "vdb_dump FAILED, res=$res output=$output" && exit 1;
fi

echo vdb_dump is finished