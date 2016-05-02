#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

ACC="SRR341578"
SCRATCH="/panfs/traces01/compress/qa/raetzw/fastdump/"
THREADS="6"

check_special()
{
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

check_special
check_fastq
