#!/bin/bash

bin_dir=$1
sra_stat=$2

echo Testing ${sra_stat}-slow from ${bin_dir}

echo slow_bases:
rm -rf actual
mkdir -p actual

echo SRR586259 is a cSRA without local references
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} -x SRR586259  | perl -w strip-path-sdlr.pl > actual/SRR586259
output=$(diff actual/SRR586259 expected/SRR586259)
res=$?
if [ "$res" != "0" ];
	then echo "slow_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR495844 is a cSRA with local references
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} -x SRR495844 | perl -w strip-path-sdlr.pl  > actual/SRR495844
output=$(diff actual/SRR495844 expected/SRR495844-with-Changes)
res=$?
if [ "$res" != "0" ];
	then echo "slow_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR390427 is a non-cSRA DB
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} -x SRR390427 > actual/SRR390427
output=$(diff actual/SRR390427 expected/SRR390427)
res=$?
if [ "$res" != "0" ];
	then echo "slow_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR360929 is a table
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} -x SRR360929 > actual/SRR360929
output=$(diff actual/SRR360929 expected/SRR360929-biological)
res=$?
if [ "$res" != "0" ];
	then echo "slow_bases test FAILED, res=$res output=$output" && exit 1;
fi

rm -rf actual
echo slow_bases test is finished
