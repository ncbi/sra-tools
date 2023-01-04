# ================================================================
#
#   Test #2 : cSRA
#
# ================================================================

set -e

BINDIR="$1"

RND_DATA_SINGLE_FILE="random_data.csra"

#-----------------------------------------------------------------
# create the random cSRA-file ( if it does not exist - yet )

function create_random_csra_data {
echo "creating: $RANDOM_CSRA_FILE"

#-----------------------------------------------------------------
# with the help of the sam-factory we produce random input for bam-load
SAMFACTORY="${BINDIR}/sam-factory"
if [[ ! -x $SAMFACTORY ]]; then
    echo "${SAMFACTORY} not found - exiting..."
    exit 3
fi

RNDREF="random-ref.FASTA"
RNDSAM="random_sam.SAM"
rm -rf $RNDREF $RNDSAM
#with the help of HEREDOC we pipe the configuration into
# the sam-factory-tool via stdin to produce 2 alignment-pairs
# and one unaligned record, in total 5 entries, all without qualities
$SAMFACTORY << EOF
r:type=random,name=R1,length=3000
ref-out:$RNDREF
sam-out:$RNDSAM
p:name=A,qual=*,repeat=10
p:name=A,qual=*,repeat=10
EOF

if [[ ! -f $RNDREF ]]; then
    echo "${RNDREF} not produced - exiting..."
    exit 3
fi

if [[ ! -f $RNDSAM ]]; then
    echo "${RNDSAM} not produced - exiting..."
    exit 3
fi

#-----------------------------------------------------------------
# load the random data produced above with bam-load

BAMLOAD="${BINDIR}/bam-load"
if [[ ! -x $BAMLOAD ]]; then
    echo "${BAMLOAD} not found - exiting..."
    exit 3
fi

RND_DATA_DIR="random-data.dir"
rm -rf "${RND_DATA_DIR}"
$BAMLOAD $RNDSAM --ref-file $RNDREF --output $RND_DATA_DIR

if [[ ! -d $RND_DATA_DIR ]]; then
    echo "${RND_DATA_DIR} not produced - exiting..."
    exit 3
else
    rm -rf $RNDSAM $RNDREF
fi

#-----------------------------------------------------------------
# kar the output of bam-load into a single file archive

KAR="${BINDIR}/kar"
if [[ ! -x $KAR ]]; then
    echo "${KAR} not found - exiting..."
    exit 3
fi

RND_DATA_SINGLE_FILE="random_data.csra"
rm -f "${RND_DATA_SINGLE_FILE}"
$KAR -c $RND_DATA_SINGLE_FILE -d $RND_DATA_DIR

if [[ ! -f $RND_DATA_SINGLE_FILE ]]; then
    echo "${RND_DATA_SINGLE_FILE} not produced - exiting..."
    exit 3
else
    rm -rf $RND_DATA_DIR
fi

}   #end of function create_random_csra_data

if [[ ! -f $RND_DATA_SINGLE_FILE ]]; then
    create_random_csra_data
fi

# ================================================================
# >>>> the actual test if fasterq-dump does produce
#       a FASTA-defline, that contains the READ-ID

FASTERQDUMP="${BINDIR}/fasterq-dump"
if [[ ! -x $FASTERQDUMP ]]; then
    echo "${FASTERQDUMP} not found - exiting..."
    rm -f $RND_DATA_SINGLE_FILE
    exit 3
fi

$FASTERQDUMP $RND_DATA_SINGLE_FILE --fasta-unsorted -f

#-----------------------------------------------------------------
# now count how many /1 and /2 reads we have

RND_DATA_FASTA="$RND_DATA_SINGLE_FILE.fasta"

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
done < $RND_DATA_FASTA

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

rm -f $RND_DATA_FASTA
echo "success testing read-id for --fasta-unsorted mode on cSRA table"
