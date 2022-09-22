#! /usr/bin/bash

set -e

ACC=`./acc.sh`
VDBDUMP=`./vdb-dump.sh`

function compile {
    g++ -o $1 -O3 $1.cpp
}

compile "extract_row_count"
compile "step1"

ALIGN_ROW_COUNT=`$VDBDUMP $ACC -T PRIM -C GLOBAL_REF_START -r | ./extract_row_count`

QUAL="'(INSDC:quality:text:phred_33)QUALITY'"
RDLEN="READ_LEN"
ALID="PRIMARY_ALIGNMENT_ID"
RDFLT="READ_FILTER"

# -n ... without sra-types conversion ( because READ_FILTER )

CMD="${VDBDUMP} ${ACC} -C ${QUAL},${RDLEN},${ALID},${RDFLT} -n -f tab | ./step1 ${ALIGN_ROW_COUNT} SRR_TST"
#echo "$CMD"
eval "$CMD"
