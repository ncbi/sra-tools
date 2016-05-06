#!/bin/bash

ACC="$1"
SCRATCH="$2"
THREADS="$3"
BINDIR="$4"

echo ""
echo "===== TESTING FASTDUMP: TEST #1 ( output-type = SPOT_ID,READ,SPOT_GROUP )=="
echo "accession       : $ACC"
echo "scratch-space at: $SCRATCH"
echo "parallel threads: $THREADS"
echo "binaries in     : $BINDIR"
echo ""

FASTDUMP_OUT="$SCRATCH$ACC.fastdump.txt"
VDB_DUMP_OUT="$SCRATCH$ACC.vdb_dump.txt"

clear_files()
{
    CMD="rm -rf $FASTDUMP_OUT $VDB_DUMP_OUT"
    $CMD 2>&1 > /dev/null
}

clear_files

#produce the output using the lookup-file
CMD="$BINDIR/fastdump $ACC -t $SCRATCH -f special -o $FASTDUMP_OUT -e $THREADS -p"
echo "$CMD"
$CMD
rc=$?; if [[ $rc != 0 ]]; then echo "$CMD failed"; exit $rc; fi

#produce the same output using vdb-dump with internal schema-joins
CMD="$BINDIR/vdb-dump $ACC -C SPOT_ID,READ,SPOT_GROUP -f tab"
echo "$CMD"
$CMD > $VDB_DUMP_OUT
rc=$?; if [[ $rc != 0 ]]; then echo "$CMD failed"; exit $rc; fi

#verify that the output of fastdump via vdb-dump
CMD="diff $FASTDUMP_OUT $VDB_DUMP_OUT"
echo "$CMD"
$CMD 2>&1 > /dev/null
rc=$?;
if [[ $rc != 0 ]]; then echo "$CMD failed"; exit $rc; fi
if [[ $rc == 0 ]]; then echo ">>>SUCCESS!"; fi

clear_files

exit $rc
