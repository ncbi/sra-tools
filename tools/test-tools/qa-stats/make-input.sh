#!/bin/sh

VDBDUMP=$(which vdb-dump) || exit $?

[ "${1}" == "" ] && { echo "usage: ${0} <path or accession>"; exit 1; }

${VDBDUMP} -E ${1} | grep -q -e "tbl\s*:\s*PRIMARY_ALIGNMENT" \
&& ${VDBDUMP} -f tab ${1} -T PRIMARY_ALIGNMENT -C "READ,REF_NAME,REF_POS,REF_ORIENTATION,CIGAR_SHORT,SEQ_SPOT_GROUP"

${VDBDUMP} -E ${1} | grep -q -e "tbl\s*:\s*SECONDARY_ALIGNMENT" \
&& ${VDBDUMP} -f tab ${1} -T SECONDARY_ALIGNMENT -C "READ,REF_NAME,REF_POS,REF_ORIENTATION,CIGAR_SHORT,SEQ_SPOT_GROUP"

${VDBDUMP} -o ${1} | grep -q -e "^CMP_READ" \
&& ${VDBDUMP} -o ${1} | grep -q -e "^PRIMARY_ALIGNMENT_ID" \
&& ${VDBDUMP} -f tab ${1} -C "CMP_READ,READ_LEN,READ_START,READ_TYPE,PRIMARY_ALIGNMENT_ID,SPOT_GROUP" \
&& exit 0

${VDBDUMP} -f tab ${1} -C "READ,READ_LEN,READ_START,READ_TYPE,SPOT_GROUP"
