# ================================================================
#
#   Test #1 : flat table
#
#   to keep the test quick we use a slice of ERR3487613
#   we are using ERR3487613.short - which has the first 10
#   spots of the original accession
#
# ================================================================

set -e

BINDIR="$1"

FASTERQDUMP="${BINDIR}/fasterq-dump"
if [[ ! -x $FASTERQDUMP ]]; then
    echo "${FASTERQDUMP} not found - exiting..."
    exit 3
fi

FLAT_TABLE_ACC="ERR3487613"

if [[ ! -f $FLAT_TABLE_ACC ]]; then
    echo "${FLAT_TABLE_ACC} not found here - recreating it"

    VDB_COPY="${BINDIR}/vdb-copy"
    if [[ ! -x $VDB_COPY ]]; then
        echo "${VDB_COPY} not found - exiting..."
        exit 3
    fi
    FLAT_TABLE_SHORT_DIR="${FLAT_TABLE}.dir"
    CMD="${VDB_COPY} ${FLAT_TABLE_ACC} ${FLAT_TABLE_SHORT_DIR} -R 1-10"
    eval "${CMD}"

    KAR="${BINDIR}/kar"
    if [[ ! -x $KAR ]]; then
        echo "${KAR} not found - exiting..."
        exit 3
    fi
    CMD="${KAR} -c ${FLAT_TABLE_ACC} -d ${FLAT_TABLE_SHORT_DIR}"
    eval "${CMD}"

    rm -rf "${FLAT_TABLE_SHORT_DIR}"
fi

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

