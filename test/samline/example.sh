#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

BAMLOAD_BINARY="bam-load"
SAMTOOLS_BINARY="/netopt/ncbi_tools64/samtools/bin/samtools"
SAMLINE_BINARY="samline"
    
# call: produce_SAM "$CONFIG" "$SAMFILE"
produce_SAM_and_Config()
{
	REFNAME="NC_011752.1"
	REFPOS1=1000
	REFPOS2=3500
	CIGAR1="30MAAA20M"
	CIGAR2="50M2D10M"
	execute "$SAMLINE_BINARY -r $REFNAME -p $REFPOS1 -p $REFPOS2 -c $CIGAR1 -c $CIGAR2 -o -o -d -n $1 > $2"
}

produce_SAM()
{
	REFNAME="NC_011752.1"
	REFPOS1=1000
	REFPOS2=3500
	CIGAR1="30MAAA20M"
	CIGAR2="50M2D10M"
	execute "$SAMLINE_BINARY -r $REFNAME -p $REFPOS1 -p $REFPOS2 -c $CIGAR1 -c $CIGAR2 -o -o -d > $1"
}

# call: convert_SAM_to_BAM "$SAMFILE" "$BAMFILE"
convert_SAM_to_BAM()
{
	execute "$SAMTOOLS_BINARY view -bS $1 > $2"
}

# call: load_BAM_to_CSRA_with_Config "$CONFIG" "$BAMFILE" "$TEMP_DIR"
load_BAM_to_CSRA_with_Config()
{
	execute "$BAMLOAD_BINARY -L 3 -o $3 -k $1 -E0 -Q0 $2"
}

# call: load_BAM_to_CSRA "$BAMFILE" "$TEMP_DIR"
load_BAM_to_CSRA()
{
	execute "$BAMLOAD_BINARY -L 3 -o $2 -E0 -Q0 $1"
}

# call: load_SAM_to_CSRA "$CONFIG" "$SAMFILE" "$TEMP_DIR"
load_SAM_to_CSRA()
{
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


#produce_SAM_and_Config "$CONFIG" "$SAMFILE"
produce_SAM "$SAMFILE"
convert_SAM_to_BAM "$SAMFILE" "$BAMFILE"

load_BAM_to_CSRA "$BAMFILE" "$TEMP_DIR"
#load_BAM_to_CSRA_with_Config "$CONFIG" "$BAMFILE" "$TEMP_DIR"
#load_SAM_to_CSRA "$CONFIG" "$SAMFILE" "$TEMP_DIR"
kar_CSRA "$FINAL_CSRA" "$TEMP_DIR"

#execute "rm -rf $TEMP_DIR $SAMFILE $BAMFILE $CONFIG"
execute "vdb-dump $FINAL_CSRA --info"
execute "sam-dump $FINAL_CSRA > final.SAM"
execute "vdb-dump --diff temp.SAM final.SAM > diff.txt"
