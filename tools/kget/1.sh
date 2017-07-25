#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

URL="https://ftp-trace.ncbi.nlm.nih.gov/giab/ftp/data/AshkenazimTrio/HG002_NA24385_son/NIST_HiSeq_HG002_Homogeneity-10953946/NHGRI_Illumina300X_AJtrio_novoalign_bams/HG002.GRCh38.300x.bam"

CACHE_KEY="/repository/user/cache-disabled"

MB10="10000000"
MB20="20000000"
MB40="40000000"

TIMECMD="`which time` -f '%E'"

BIGBLOCK="5242880"

cache_on()
{
    eval "vdb-config -s $CACHE_KEY=false"
}

cache_off()
{
    eval "vdb-config -s $CACHE_KEY=true"
}

direct_read()
{
    SRC=$1
    BYTES=$2
    CMD="$TIMECMD kget $SRC --quiet --function vfs --count $BYTES"
    echo "------------------------------------------------------"
    echo "direct read of $BYTES bytes"
    eval $CMD
    echo "."    
}

direct()
{
    cache_off
    direct_read $URL $MB10
    direct_read $URL $MB20
    direct_read $URL $MB40    
}

cached_read()
{
    SRC=$1
    BYTES=$2
    CMD="$TIMECMD kget $SRC --quiet --function vfs --count $BYTES"
    echo "------------------------------------------------------"
    echo "cached read of $BYTES bytes"
    eval "cache-mgr -c > /dev/null"
    eval $CMD
    echo "."    
}

cached()
{
    cache_on
    cached_read $URL $MB10
    cached_read $URL $MB20
    cached_read $URL $MB40
}

cached_read_big_block()
{
    SRC=$1
    BYTES=$2
    CMD="$TIMECMD kget $SRC --quiet --function vfs --count $BYTES --block-size $BIGBLOCK"
    echo "------------------------------------------------------"
    echo "cached read of $BYTES bytes with blocksize of $BIGBLOCK bytes"
    eval "cache-mgr -c > /dev/null"
    eval $CMD
    echo "."    
}

cached_big_block()
{
    cache_on
    cached_read_big_block $URL $MB10
    cached_read_big_block $URL $MB20
    cached_read_big_block $URL $MB40
}

direct
cached
cached_big_block
