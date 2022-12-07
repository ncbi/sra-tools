# ================================================================
#
#   Test #2 : cSRA
#
#   to keep the test quick we use a slice of SRR341578
#   the slice is made by using sam-dump on a short reference-slice
#   (NC_011748.1:2300001-2300005)
#   this is then 'loaded' with bam-load
#
# ================================================================

set -e

BINDIR="$1"

FASTERQDUMP="${BINDIR}/fasterq-dump"
if [[ ! -x $FASTERQDUMP ]]; then
    echo "${FASTERQDUMP} not found - exiting..."
    exit 3
fi

CSRA_ACC="SRR341578"

if [[ ! -f $CSRA_ACC ]]; then
    echo "${CSRA_ACC} not found here - recreating it"
    CSRA_SLICE_SAM="${CSRA_ACC}.SAM"

    if [[ ! -f $CSRA_SLICE_SAM ]]; then
        echo "${CSRA_SLICE_SAM} not found here - recreating it"
        SAM_DUMP="${BINDIR}/sam-dump"
        if [[ ! -x $SAM_DUMP ]]; then
            echo "${SAM_DUMP} not found - exiting..."
            exit 3
        fi

        #SLICE="--aligned-region NC_011748.1:2300001-2300005 -r"
        SLICE="--aligned-region NC_011752.1:40001-40010 -r"
        CMD="${SAM_DUMP} ${CSRA_ACC} ${SLICE} > ${CSRA_SLICE_SAM}"
        eval "${CMD}"
    else
        echo "${CSRA_SLICE_SAM} found here - no need to recreate it"
    fi


fi

exit 0

FLAT_TABLE_FASTA="${FLAT_TABLE_ACC}.fasta"
#run fasterq-dump on the short slice and produce unsorted FASTA
CMD="${FASTERQDUMP} ./${FLAT_TABLE_ACC} --fasta-unsorted -f"
eval "${CMD}"

READ_ID1=0
READ_ID2=0
#read the produced FASTA-file - line by line
while read -r LINE; do
    #if LINE starts with '>'
    if [[ $LINE == \>* ]]; then
        #cut out the first 'word' aka ACCESSION.ROW_ID/READ_ID
        LINEARRAY=($LINE)
        FIRST_WORD="${LINEARRAY[0]}"
        #count how many /1 and /2 we encounter
        if [[ $FIRST_WORD == */1 ]]; then
            READ_ID1=$(( READ_ID1 + 1 ))
        fi
        if [[ $FIRST_WORD == */2 ]]; then
            READ_ID2=$(( READ_ID2 + 1 ))
        fi
    fi
done <$FLAT_TABLE_FASTA

if [[ ! $READ_ID1 -eq 10 ]]; then
    echo "error number of '/1' reads should be 10, but it is ${READ_ID1}"
    exit 1
else
    echo "READ_ID1: ${READ_ID1}"
fi

if [[ ! $READ_ID2 -eq 10 ]]; then
    echo "error number of '/2' reads should be 10, but it is ${READ_ID2}"
    exit 1
else
    echo "READ_ID2: ${READ_ID2}"
fi

echo "success testing read-id for --fasta-unsorted mode on flat table"

