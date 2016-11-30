#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

# call: produce_SAM "$CONFIG" "$SAMFILE"
produce_SAM()
{
    SAMLINE_BINARY="samline"
    REFNAME="NC_011752.1"

    REFPOS1=1000
    REFPOS2=3500
    CIGAR1="30MAAA20M"
    CIGAR2="50M2D10M"
    QNAME="1"
    execute "$SAMLINE_BINARY --qname $QNAME --qname $QNAME -r $REFNAME -p $REFPOS1 -p $REFPOS2 -c $CIGAR1 -c $CIGAR2 -n $1 -d >> $2"
    
    REFPOS1=4200
    REFPOS2=4300
    CIGAR1="30M3I10M"
    CIGAR2="70M"
    QNAME="2"
    execute "$SAMLINE_BINARY --qname $QNAME --qname $QNAME -r $REFNAME -p $REFPOS1 -p $REFPOS2 -c $CIGAR1 -c $CIGAR2 -i TTT >> $2"
    
}

# call: convert_SAM_to_BAM "$SAMFILE" "$BAMFILE"
convert_SAM_to_BAM()
{
    SAMTOOLS_BINARY="/netopt/ncbi_tools64/samtools/bin/samtools"
    execute "$SAMTOOLS_BINARY view -bS $1 > $2"
}

# call: load_BAM_to_CSRA "$CONFIG" "$BAMFILE" "$TEMP_DIR"
load_BAM_to_CSRA()
{
    BAMLOAD_BINARY="bam-load"
    execute "$BAMLOAD_BINARY -L 3 -o $3 -k $1 -E0 -Q0 $2"
}

# call: load_SAM_to_CSRA "$CONFIG" "$SAMFILE" "$TEMP_DIR"
load_SAM_to_CSRA()
{
    BAMLOAD_BINARY="bam-load"
    execute "cat $2 | $BAMLOAD_BINARY -L 3 -o $3 -k $1 -E0 -Q0 /dev/stdin"
}

# call: kar_CSRA "$FINAL_CSRA" "$TEMP_DIR"
kar_CSRA()
{
    KAR_BINARY="kar"
    execute "$KAR_BINARY --create $1 -d $2 -f"
}

SAMFILE="temp.SAM"
BAMFILE="temp.BAM"
CONFIG="temp.kfg"
TEMP_DIR="temp_csra"
FINAL_CSRA="test.csra"

produce_SAM "$CONFIG" "$SAMFILE"
#convert_SAM_to_BAM "$SAMFILE" "$BAMFILE"
#load_BAM_to_CSRA "$CONFIG" "$BAMFILE" "$TEMP_DIR"
load_SAM_to_CSRA "$CONFIG" "$SAMFILE" "$TEMP_DIR"
kar_CSRA "$FINAL_CSRA" "$TEMP_DIR"

execute "rm -rf $TEMP_DIR $SAMFILE $BAMFILE $CONFIG $FINAL_CSRA"
#execute "vdb-dump $FINAL_CSRA --info"
cat var-expand.log
