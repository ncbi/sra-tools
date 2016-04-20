#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

# call: produce_SAM "$SAMFILE" "$CONFIG"
produce_SAM()
{
    SAMLINE_BINARY="samline"
    
    OUTFILE=$1
    CONFIG=$2

    REFNAME="NC_011752.1"
    REFALIAS0="c1"
    REFALIAS1="c2"
    REFPOS0=1000
    REFPOS1=3500
    CIGAR0="50M"
    CIGAR1="50M"
    
    ALIG0="-r $REFNAME -b $REFALIAS0 -p $REFPOS0 -c $CIGAR0"
    ALIG1="-r $REFNAME -b $REFALIAS1 -p $REFPOS1 -c $CIGAR1"
    #ALIG0="-r $REFNAME -p $REFPOS0 -c $CIGAR0"
    #ALIG1="-r $REFNAME -p $REFPOS1 -c $CIGAR1"
    WRITE_CONFIG="-n $CONFIG"
    WRITE_HDR="-d" 
    
    execute "$SAMLINE_BINARY $ALIG0 $ALIG1 $WRITE_HDR $WRITE_CONFIG > $OUTFILE"
}

# call: load_SAM_to_CSRA "$CONFIG" "$SAMFILE" "$TEMP_DIR"
load_SAM_to_CSRA()
{
    BAMLOAD_BINARY="bam-load"
    execute "cat $2 | $BAMLOAD_BINARY -L 5 -o $3 -k $1 -E0 -Q0 /dev/stdin" # --allow-multi-map"
}

# call: kar_CSRA "$FINAL_CSRA" "$TEMP_DIR"
kar_CSRA()
{
    KAR_BINARY="kar"
    execute "$KAR_BINARY --create $1 -d $2 -f"
}

PREFIX="RMF"
SAMFILE="${PREFIX}.SAM"
CONFIG="${PREFIX}.kfg"
TEMP_DIR="${PREFIX}_csra"
FINAL_CSRA="${PREFIX}.csra"

#produce_SAM "$SAMFILE" "$CONFIG"
#execute "rm -rf $TEMP_DIR"
load_SAM_to_CSRA "$CONFIG" "$SAMFILE" "$TEMP_DIR"
#kar_CSRA "$FINAL_CSRA" "$TEMP_DIR"
#execute "rm -rf $TEMP_DIR"
#execute "vdb-dump $FINAL_CSRA --info"