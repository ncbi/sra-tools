#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

#produce 1 alignment against reference NC_011752.1 at position 1000 (1based) with cigar-string 50M
REFNAME="NC_011752.1"
REFPOS1=1000
REFPOS2=6000
CIGAR1="30MAAA20M"
CIGAR2="100M"
CONFIG="config.kfg"

SAMLINE="samline -r $REFNAME -p $REFPOS1 -p $REFPOS2 -c $CIGAR1 -c $CIGAR2 -n $CONFIG -d"

#load this alignment with bam-load
OUTDIR="test"
LOADLINE="bam-load -o $OUTDIR -k $CONFIG -E0 -Q0 /dev/stdin"

#kar it up
OUTFILE="test.sra"
KARLINE="kar --create $OUTFILE -d $OUTDIR -f"

#print vdb-dump to show success
DUMPLINE="vdb-dump $OUTFILE --info"

#remove temp. directory
RMLINE="rm -rf $OUTDIR"

#execute "$SAMLINE"
execute "$SAMLINE | $LOADLINE && $KARLINE && $DUMPLINE && $RMLINE"
