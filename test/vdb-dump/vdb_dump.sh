#!/bin/bash

vdb_dump_makedb=$1
bin_dir=$2

echo vdb-dump-makedb: ${vdb_dump_makedb}
echo cmake_source_dir: ${cmake_source_dir}

echo "testing vdb_dump"

TEMPDIR=.

echo Testing vdb-dump from ${bin_dir}

rm -rf actual
mkdir -p actual

echo makedb:
rm -rf data; mkdir -p data; ${vdb_dump_makedb}

echo output format
output=$(NCBI_SETTINGS=/ ${bin_dir}/vdb-dump SRR056386 -R 1 -C READ -f tab > actual/1.0.stdout && diff expected/1.0.stdout actual/1.0.stdout)
res=$?
if [ "$res" != "0" ];
	then echo "vdb_dump 1.0 FAILED, res=$res output=$output" && exit 1;
fi

output=$(NCBI_SETTINGS=/ ${bin_dir}/vdb-dump SRR056386 -R 1 -C READ -f tab -I > actual/1.1.stdout && diff expected/1.1.stdout actual/1.1.stdout)
res=$?
if [ "$res" != "0" ];
	then echo "vdb_dump 1.1 FAILED, res=$res output=$output" && exit 1;
fi


echo nested databases
output=$(NCBI_SETTINGS=/ ${bin_dir}/vdb-dump -E data/NestedDatabase > actual/2.0.stdout && diff expected/2.0.stdout actual/2.0.stdout)
res=$?
if [ "$res" != "0" ];
	then echo "vdb_dump 2.0 FAILED, res=$res output=$output" && exit 1;
fi

output=$(NCBI_SETTINGS=/ ${bin_dir}/vdb-dump -T SUBDB_1.SUBSUBDB_1.TABLE1 data/NestedDatabase > actual/2.1.stdout && diff expected/2.1.stdout actual/2.1.stdout)
res=$?
if [ "$res" != "0" ];
	then echo "vdb_dump 2.1 FAILED, res=$res output=$output" && exit 1;
fi

output=$(NCBI_SETTINGS=/ ${bin_dir}/vdb-dump -T SUBDB_1.SUBSUBDB_2.TABLE2 data/NestedDatabase > actual/2.2.stdout && diff expected/2.2.stdout actual/2.2.stdout)
res=$?
if [ "$res" != "0" ];
	then echo "vdb_dump 2.2 FAILED, res=$res output=$output" && exit 1;
fi

rm -rf actual
rm -rf data
./test_buffer_insufficient.sh ${bin_dir}/vdb-dump VDB-3937.kar
res=$?
if [ "$res" != "0" ];
	then echo "test_buffer_insufficient FAILED, res=$res output=$output" && exit 1;
fi
echo "...all tests passed"

echo vdb_dump is finished