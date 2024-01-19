#!/bin/bash

bin_dir=$1
sra_stat=$2

echo Testing ${sra_stat} from ${bin_dir}

echo quick_bases:

echo SRR22714250 is a small DB with default SPOT_GROUP
rm -rf actual
mkdir -p actual
NCBI_SETTINGS=/ ${bin_dir}/${sra_stat} -x db/SRR22714250.lite.1 > actual/SRR22714250
output=$(diff actual/SRR22714250 expected/SRR22714250-default-SPOT_GROUP)
res=$?
if [ "$res" != "0" ];
	then echo "quick_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR053325 is a small table
rm -rf actual
mkdir -p actual
NCBI_SETTINGS=/ ${bin_dir}/${sra_stat} -x SRR053325 > actual/SRR053325
output=$(diff actual/SRR053325 expected/SRR053325-biological-reloaded)
res=$?
if [ "$res" != "0" ];
	then echo "quick_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR600096 is a small non-cSRA DB
rm -rf actual
mkdir -p actual
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} -x SRR600096 > actual/SRR600096
output=$(diff actual/SRR600096 expected/SRR600096)
res=$?
if [ "$res" != "0" ];
	then echo "quick_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR618333 is a small CS_NATIVE table
NCBI_SETTINGS=/ ${bin_dir}/${sra_stat} -x SRR618333 > actual/SRR618333
output=$(diff actual/SRR618333 expected/SRR618333)
res=$?
if [ "$res" != "0" ];
	then echo "quick_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR413283 is a small cSRA with local references
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} -x SRR413283 > actual/SRR413283
output=$(diff actual/SRR413283 expected/SRR413283-with-AssemblyStatistics)
res=$?
if [ "$res" != "0" ];
	then echo "quick_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR619505 is a small cSRA with N-s without local references
NCBI_SETTINGS=/ ${bin_dir}/${sra_stat} -x SRR619505 | perl -w strip-path-sdlr.pl > actual/SRR619505
output=$(diff actual/SRR619505 expected/SRR619505)
res=$?
if [ "$res" != "0" ];
	then echo "quick_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo SRR1985136 is a small cSRA with a local reference and 0-length tech-rd
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} -x SRR1985136 | perl -w strip-path.pl > actual/SRR1985136
output=$(diff actual/SRR1985136 expected/SRR1985136-with-Changes)
res=$?
if [ "$res" != "0" ];
	then echo "quick_bases test FAILED, res=$res output=$output" && exit 1;
fi

echo check SOFTWARE node for a table
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} --quick -x --meta SRR053325 > actual/SRR053325
output=$(diff actual/SRR053325 expected/SRR053325-meta)
res=$?
if [ "$res" != "0" ];
	then echo "table SOFTWARE test FAILED, res=$res output=$output" && exit 1;
fi

echo check SOFTWARE node for a DB
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R ${bin_dir}/${sra_stat} --quick -x --meta SRR19599626 > actual/SRR19599626
output=$(diff actual/SRR19599626 expected/SRR19599626-meta)
res=$?
if [ "$res" != "0" ];
        then echo "DB SOFTWARE test FAILED, res=$res output=$output" && exit 1;
fi

rm actual/*
echo quick_bases test is finished

R=SRR8483030
echo test_bases:
echo ${R} is a run having first 0-lenght bio reads
if perl check-run-perm.pl ${bin_dir}/srapath ${R} > /dev/null 2>&1 ; \
 then                      ${bin_dir}/${sra_stat} -x ${R} > /dev/null ; fi
res=$?
if [ "$res" != "0" ];
	then echo "test_bases test FAILED, res=$res output=$output" && exit 1;
fi

rm -rf actual
echo test_bases test is finished
