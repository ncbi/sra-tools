#!/bin/bash

OLD=/panfs/traces01.be-md.ncbi.nlm.nih.gov/trace_software/toolkit/centos64/bin/latf-load
NEW=latf-load
TOP=/panfs/pan1.be-md.ncbi.nlm.nih.gov/trace_work/backup/qa/cases/load
OUT=/panfs/pan1.be-md.ncbi.nlm.nih.gov/sra-test/latf-load/qq

function run {
    echo "$1 $2"
    rm -rf $OUT
    printf "old: $OLD --quality PHRED_33 -o $OUT $1 $2"
    time $OLD --quality PHRED_33 -o $OUT $1 $2
    du -s $OUT

    rm -rf $OUT
    printf "new:"
    time $NEW --quality PHRED_33 -o $OUT $1 $2
    du -s $OUT

    echo
}

#run $TOP/latf/SRR529889/PmaximaFP.fastq
#run $TOP/latf/SRR1692309/Hatch66-3_R1_filtered.fastq $TOP/latf/SRR1692309/Hatch66-3_R2_filtered.fastq
#run $TOP/latf/SRR567550/s_7_1_sequence_100409.fq.gz $TOP/latf/SRR567550/s_7_2_sequence_100409.fq.gz
#run $TOP/latf/XXX034449/S.cerevisiae_Fragmented_Replicate2.fasta
#run $TOP/latf/SRR627950/1920.1.1690.fastq
#run $TOP/genericFastq/SRR8054133/TICK01_R1.fastq.gz $TOP/genericFastq/SRR8054133/TICK01_R2.fastq.gz
run $TOP/genericFastq/SRR3020730/sim_CFSAN001140_2.fq
run $TOP/latf/SRR989791/XSBOR_20130726_RS42150_PL100036291-1_C02.fastq $TOP/latf/SRR989791/XSBOR_20130726_RS42150_PL100036291-1_D02.fastq




