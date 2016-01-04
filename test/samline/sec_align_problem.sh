#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

# this will produce 2 files: 
#   a) a config-file for bam-load with the used reference
#   b) a SAM-file with header-line containing the lonely sec. alignment
# call: produce_SAM "$CONFIG" "$SAMFILE"
produce_SAM1()
{
	SAMLINE_BINARY="samline"
    QNAME="--qname 1"
	REFNAME="-r NC_011752.1"
	REFPOS="-p 6800"
	CIGAR="-c 55M"
    SECONDARY="-2 1"
    #FIRST="--first 1"
    WITH_HEADERS="-d"
    CREATE_CONFIG="-n $1"
	execute "$SAMLINE_BINARY $QNAME $REFNAME $REFPOS $CIGAR $SECONDARY $FIRST $WITH_HEADERS $CREATE_CONFIG > $2"
}

# this will produce 1 file with these 2 primary mates
# call: produce_SAM "$SAMFILE"
produce_SAM2()
{
	SAMLINE_BINARY="samline"
    QNAME="--qname 2"
	REFNAME="-r NC_011752.1"
	REFPOS1="-p 1000"
	REFPOS2="-p 3500"
	CIGAR1="-c 30MAAA20M"
	CIGAR2="-c 50M2D10M"
    SEC1="-2 0"
    SEC2="-2 1"
	execute "$SAMLINE_BINARY $QNAME $REFNAME $REFPOS1 $REFPOS2 $CIGAR1 $CIGAR2 $SEC1 $SEC2 >> $1"
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

SAMFILE="p1.SAM"
CONFIG="p1.cfg"
TEMP_DIR="p1_csra"
FINAL_CSRA="p1.csra"
TEMP_SORTED_CSRA="p2_csra"
SORTED_CSRA="p2.csra"

produce_SAM1 "$CONFIG" "$SAMFILE"
produce_SAM2 "$SAMFILE"
load_SAM_to_CSRA "$CONFIG" "$SAMFILE" "$TEMP_DIR"
kar_CSRA "$FINAL_CSRA" "$TEMP_DIR"
#execute "vdb-dump $FINAL_CSRA --info"
#execute "sra-stat $FINAL_CSRA"
#execute "sra-sort $FINAL_CSRA $TEMP_SORTED_CSRA -f"
#kar_CSRA "$SORTED_CSRA" "$TEMP_SORTED_CSRA"
#execute "vdb-dump $SORTED_CSRA --info"
#execute "sra-stat $SORTED_CSRA"
#execute "rm -rf $TEMP_DIR $TEMP_SORTED_CSRA $SAMFILE $CONFIG"
