#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

ACC="SRR341578"
LOOKUP="SRR341578.lookup"
SPECIAL="special.txt"
SPECIAL2="vdb_dump_special.txt"
SCRATCH_PATH="/panfs/traces01/compress/qa/raetzw/fastdump/"
SCRATCH="-t $SCRATCH_PATH"
INDEX_FILE="SRR341578.lookup.idx"
INDEX="-i $INDEX_FILE"

#remove lookup_file
CMD="rm -rf $LOOKUP $INDEX_FILE"
#execute "$CMD"

#produce the lookup-file
CMD="time fastdump $ACC -f lookup -o $LOOKUP $INDEX $SCRATCH -e 4 -p"
#CMD="totalview fastdump -a $ACC -f lookup -o $LOOKUP $INDEX $SCRATCH -m 4G -p"
#CMD="time fastdump $ACC -f lookup -o $LOOKUP $SCRATCH -p"
#execute "$CMD"
# about 1m26

#check key seeker
CMD="time fastdump $ACC -f test -l $LOOKUP $INDEX -a 1"
execute "$CMD"

#produce the output using the lookup-file
CMD="time fastdump $ACC -l $LOOKUP -o $SPECIAL -p"
#execute "$CMD"
#about 0m49

#produce the same output using vdb-dump with internal schema-joins
#CMD="time vdb-dump $ACC -C SPOT_ID,READ,SPOT_GROUP -f tab > $SPECIAL2"
#execute "$CMD"
# about 3m10

#verify that the output is the same
CMD="time diff $SPECIAL $SPECIAL2"
#execute "$CMD"
