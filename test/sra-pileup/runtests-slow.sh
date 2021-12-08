#!/bin/bash

bin_dir=$1
python_bin=$2

echo Testing sra-pileup from ${bin_dir} with python ${python_bin}


rm -rf actual
mkdir -p actual

echo check_skiplist:
output=$(${python_bin} check_skiplist.py ${bin_dir}/sra-pileup)
res=$?
if [ "$res" != "0" ];
	then echo "sra-pileup check_skiplist test FAILED, res=$res output=$output" && exit 1;
fi

echo fastq_dump_vs_sam_dump:
ACC=SRR3332402
output=$(${python_bin} test_diff_fastq_dump_vs_sam_dump.py -a ${ACC} -f ${bin_dir}/fastq-dump -m ${bin_dir}/sam-dump)
res=$?
if [ "$res" != "0" ];
	then echo "sra-pileup fastq_dump_vs_sam_dump test FAILED, res=$res output=$output" && exit 1;
fi

echo sam_dump_spotgroup_for_all:
output=$(${python_bin} test_all_sam_dump_has_spotgroup.py -a ${ACC} -m ${bin_dir}/sam-dump)
res=$?
if [ "$res" != "0" ];
	then echo "sra-pileup sam_dump_spotgroup_for_all test FAILED, res=$res output=$output" && exit 1;
fi

echo sra-pileup test is finished