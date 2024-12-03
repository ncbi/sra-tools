#!/usr/bin/env bash

# ================================================================
#
#   Test #2 : cSRA
#
# ================================================================

set -e

BINDIR="$1"

RND_DATA_SINGLE_FILE="random_data.csra"

FASTERQDUMP="${BINDIR}/fasterq-dump"
[ -x "${FASTERQDUMP}" ] || { echo "${FASTERQDUMP} not found - exiting..."; exit 3; }

RND_DATA_SINGLE_FILE="random_data.csra"

#-----------------------------------------------------------------
# create the random cSRA-file ( if it does not exist - yet )

[ -f "${RND_DATA_SINGLE_FILE}" ] || {
    echo "creating: ${RANDOM_CSRA_FILE}"
    
    #-----------------------------------------------------------------
    # with the help of the sam-factory we produce random input for bam-load
    SAMFACTORY="${BINDIR}/sam-factory"
    [ -x "${SAMFACTORY}" ] || { echo "${SAMFACTORY} not found - exiting (skipping)..."; exit 0; }
    
    BAMLOAD="${BINDIR}/bam-load"
    [ -x "${BAMLOAD}" ] || { echo "${BAMLOAD} not found - exiting (skipping)..."; exit 0; }
    
    KAR="${BINDIR}/kar"
    [ -x "${KAR}" ] || { echo "${KAR} not found - exiting (skipping)..."; exit 0; }
    
    RNDREF="random-ref.FASTA"
    RNDSAM="random_sam.SAM"
    rm -rf "${RNDREF}" "${RNDSAM}"
    #with the help of HEREDOC we pipe the configuration into
    # the sam-factory-tool via stdin to produce 2 alignment-pairs
    # and one unaligned record, in total 5 entries, all without qualities
    "${SAMFACTORY}" << EOF
r:type=random,name=R1,length=3000
ref-out:$RNDREF
sam-out:$RNDSAM
p:name=A,qual=*,repeat=10
p:name=A,qual=*,repeat=10
EOF
    
    [ -f "${RNDREF}" ] || { echo "${RNDREF} not produced - exiting..."; exit 3; }
    [ -f "${RNDSAM}" ] || { echo "${RNDSAM} not produced - exiting..."; exit 3; }
    
    #-----------------------------------------------------------------
    # load the random data produced above with bam-load
    
    RND_DATA_DIR="random-data.dir"
    rm -rf "${RND_DATA_DIR}"
    "${BAMLOAD}" "${RNDSAM}" --ref-file "${RNDREF}" --output "${RND_DATA_DIR}"
    
    [ -d "${RND_DATA_DIR}" ] || { echo "${RND_DATA_DIR} not produced - exiting..."; exit 3; }
    rm -rf "${RNDREF}" "${RNDSAM}"
    
    #-----------------------------------------------------------------
    # kar the output of bam-load into a single file archive
    
    rm -f "${RND_DATA_SINGLE_FILE}"
    "${KAR}" --force -c "${RND_DATA_SINGLE_FILE}" -d "${RND_DATA_DIR}"
    
    [ -f "${RND_DATA_SINGLE_FILE}" ] || { echo "${RND_DATA_SINGLE_FILE} not produced - exiting..."; exit 3; }
    rm -rf $RND_DATA_DIR
}

# ================================================================
# >>>> the actual test if fasterq-dump does produce
#       a FASTA-defline, that contains the READ-ID

"${FASTERQDUMP}" "${RND_DATA_SINGLE_FILE}" --fasta-unsorted -f

# rm -f "${RND_DATA_SINGLE_FILE}"

RND_DATA_FASTA="${RND_DATA_SINGLE_FILE}.fasta"

#-----------------------------------------------------------------
# now count how many /1 and /2 reads we have

READ_ID1=$(awk 'BEGIN{N=0} /^>/ && $1~/\/1$/ {N+=1} END{print N}' "${RND_DATA_FASTA}")
READ_ID2=$(awk 'BEGIN{N=0} /^>/ && $1~/\/2$/ {N+=1} END{print N}' "${RND_DATA_FASTA}")

[ $READ_ID1 -eq 10 ] || {
    echo "error number of '/1' reads should be 10, but it is ${READ_ID1}"
    exit 1
}
echo "READ_ID1: ${READ_ID1}"

[ $READ_ID2 -eq 10 ] || {
    echo "error number of '/2' reads should be 10, but it is ${READ_ID2}"
    exit 1
}
echo "READ_ID2: ${READ_ID2}"

rm -f "${RND_DATA_FASTA}"
echo "success testing read-id for --fasta-unsorted mode on cSRA table"
