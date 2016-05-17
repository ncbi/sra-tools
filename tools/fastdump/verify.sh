#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

ACC1="SRR341578"

#size is about 20 MB
ACC2="SRR1172940"

#size is about 200 MB
# special: 13s vs 48s, fastq: 17s vs 57s
ACC3="SRR353895"

#size is about 2 GB
#special: 1m13s vs 11m25s, fastq: 1m38s vs 11m56s
ACC4="SRR392046"

#size is about 20 GB
#special: 21m34s vs 
ACC5="SRR534041"

ACC_ONLY_1_READ="SRR449498"

SCRATCH="/panfs/traces01/compress/qa/raetzw/fastdump/"
THREADS="6"

check_special()
{
    ACC="$1"
    FASTDUMP_OUT="$SCRATCH$ACC.fastdump.special.txt"
    VDB_DUMP_OUT="$SCRATCH$ACC.vdb_dump.special.txt"
    
    #remove output
    CMD="rm -rf $FASTDUMP_OUT $VDB_DUMP_OUT"
    execute "$CMD"

    #produce the output using the lookup-file
    CMD="time fastdump $ACC -t $SCRATCH -f special -o $FASTDUMP_OUT -e $THREADS -p"
    execute "$CMD"

    #produce the same output using vdb-dump with internal schema-joins
    CMD="time vdb-dump $ACC -C SPOT_ID,READ,SPOT_GROUP -f tab > $VDB_DUMP_OUT"
    execute "$CMD"

    #verify that the output of fastdump via vdb-dump
    CMD="time diff $FASTDUMP_OUT $VDB_DUMP_OUT"
    execute "$CMD"
}

check_fastq()
{
    ACC="$1"
    FASTDUMP_OUT="$SCRATCH$ACC.fastdump.fastq.txt"
    VDB_DUMP_OUT="$SCRATCH$ACC.vdb_dump.fastq.txt"
    
    #remove output
    CMD="rm -rf $FASTDUMP_OUT $VDB_DUMP_OUT"
    execute "$CMD"

    #produce the output using the lookup-file
    CMD="time fastdump $ACC -t $SCRATCH -f fastq -o $FASTDUMP_OUT -e $THREADS -p"
    execute "$CMD"

    #produce the same output using vdb-dump with internal schema-joins
    CMD="time vdb-dump $ACC -f fastq > $VDB_DUMP_OUT"
    execute "$CMD"

    #verify that the output of fastdump via vdb-dump
    CMD="time diff $FASTDUMP_OUT $VDB_DUMP_OUT"
    execute "$CMD"
}

check_special "$ACC5"
check_fastq "$ACC5"
