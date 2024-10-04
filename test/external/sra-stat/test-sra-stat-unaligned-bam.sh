#!/bin/sh

bin_dir=$1
sra_stat=$2
latf_load=${3:-latf-load}
bam_load=${4:-bam-load}

echo Testing ${sra_stat} from ${bin_dir}
echo Using ${latf_load} and ${bam_load} from ${bin_dir}

rm -rf actual
mkdir -p actual

echo latf-load unaligned:
NCBI_SETTINGS=/ ${bin_dir}/${latf_load} \
    -o actual/from-latf-load \
    --platform ILLUMINA \
    --quality PHRED_33 \
    db/unaligned.fastq -L debug -+VDB -+KFG 2> actual/latf-load.stderr >actual/latf-load.stdout
res=$?
if [ "$res" != "0" ];
	then echo "FAILED to load with latf-load, res=$res" && exit 1;
fi
NCBI_SETTINGS=/ ${bin_dir}/${sra_stat} actual/from-latf-load >actual/stats-from-latf-load 2>actual/stats-from-latf-load.stderr
if [ "$res" != "0" ];
    then echo "FAILED to get stats from loaded with latf-load, res=$res" && exit 1;
fi

echo bam-load unaligned:
NCBI_SETTINGS=/ ${bin_dir}/${bam_load} \
    -o actual/from-bam-load \
    db/unaligned.sam 2> actual/bam-load.stderr >actual/bam-load.stdout
res=$?
if [ "$res" != "0" ];
    then echo "FAILED to load with bam-load, res=$res" && exit 1;
fi
NCBI_SETTINGS=/ ${bin_dir}/${sra_stat} actual/from-bam-load >actual/stats-from-bam-load 2>actual/stats-from-bam-load.stderr
res=$?
if [ "$res" != "0" ];
    then echo "FAILED to get stats from loaded with bam-load, res=$res" && exit 1;
fi

cat actual/stats-from-bam-load  | sed 's/bam//'  > actual/stats-from-bam-load-fixed
cat actual/stats-from-latf-load | sed 's/latf//' > actual/stats-from-latf-load-fixed

#cat actual/*-load-fixed

output=$(diff actual/stats-from-latf-load-fixed actual/stats-from-bam-load-fixed)
res=$?
if [ "$res" != "0" ];
    then echo "FAILED: sra-stat output from loaded with bam-load differ from loaded with latf-load, res=$res" && exit 1;
fi
output=$(diff actual/stats-from-latf-load.stderr actual/stats-from-bam-load.stderr)
res=$?
if [ "$res" != "0" ];
    then echo "FAILED: sra-stat stderr output from loaded with bam-load differ from loaded with latf-load, res=$res" && exit 1;
fi

rm -rf actual

