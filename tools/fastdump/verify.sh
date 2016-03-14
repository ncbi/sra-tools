#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

ACC="SRR341578"
BINLOOKUP="lookup_bin.dat"
SPECIAL="special.txt"
SPECIAL2="vdb_dump_special.txt"
SCRATCH="/panfs/traces01/compress/qa/raetzw/fastdump/"

#produce the lookup-file
CMD="time fastdump $ACC -f lookup -o $BINLOOKUP -t $SCRATCH -p -b 5M -n 8 -c 5M"
execute "$CMD"
# about 1m26

#produce the output using the lookup-file
CMD="time fastdump $ACC -l $BINLOOKUP -o $SPECIAL -b 10M -c 5M -p"
execute "$CMD"
#about 0m49

#produce the same output using vdb-dump with internal schema-joins
#CMD="time vdb-dump $ACC -C SPOT_ID,READ,SPOT_GROUP -f tab > $SPECIAL2"
#execute "$CMD"
# about 3m10

#verify that the output is the same
CMD="time diff $SPECIAL $SPECIAL2"
execute "$CMD"
