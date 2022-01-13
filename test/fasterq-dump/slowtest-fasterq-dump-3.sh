#------------------------------------------------------------------------------------------
#
#   test pre-flight-mode for fasterq-dump in different modes
#   on a flat table and a cSRA accession
#
#------------------------------------------------------------------------------------------

TOOL="fasterq-dump"

function run_limited {
    CMD="$1"
    LIMIT="$2"
    TO_DELETE="$3"
    echo "$CMD $LIMIT"
    eval "$CMD $LIMIT"
    RES="$?"
    echo "result = $RES"
    if [ $RES -eq 0 ]; then
        echo "this should not be zero!"
        rm -f "$TO_DELETE"
        exit 3
    fi
}

LIMIT="--disk-limit 10M"

#------------------------------------------------------------------------------------------
#    WHOLE SPOT
#------------------------------------------------------------------------------------------

function test_whole_spot_fastq {
    SACC=`basename $1`
    echo "" && echo "testing: WHOLE SPOT / FASTQ for ${SACC}"
    #run fasterq-dump with a disk-limit of 10M - this should fail!
    CMD="$TOOL $1 --include-technical --concatenate-reads -pf"
    run_limited "$CMD" "$LIMIT" "${SACC}.fastq"
    CMD="$TOOL $1 --include-technical --concatenate-reads -pf -o ${SACC}.faster.fastq"
    run_limited "$CMD" "$LIMIT" "${SACC}.faster.fastq"
}

function test_whole_spot_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: WHOLE SPOT / FASTA for ${SACC}"
    #run fasterq-dump
    CMD="$TOOL $1 --include-technical --concatenate-reads --fasta -pf"
    run_limited "$CMD" "$LIMIT" "${SACC}.fasta"
    CMD="$TOOL $1 --include-technical --concatenate-reads --fasta -pf -o ${SACC}.faster.fasta"
    run_limited "$CMD" "$LIMIT" "${SACC}.faster.fasta"
}

#------------------------------------------------------------------------------------------
#    SPLIT SPOT
#------------------------------------------------------------------------------------------

function test_split_spot_fastq {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT SPOT / FASTQ for ${SACC}"
    #run fasterq-dump
    CMD="$TOOL $1 --split-spot -pf"
    run_limited "$CMD" "$LIMIT" "${SACC}.fastq"
    CMD="$TOOL $1 --split-spot -pf -o ${SACC}.faster.fastq"
    run_limited "$CMD" "$LIMIT" "${SACC}.faster.fastq"
}

function test_split_spot_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT SPOT / FASTA for ${SACC}"
    #run fasterq-dump
    CMD="$TOOL $1 --split-spot --fasta -pf"
    run_limited "$CMD" "$LIMIT" "${SACC}.fasta"
    CMD="$TOOL $1 --split-spot --fasta -pf -o ${SACC}.faster.fasta"
    run_limited "$CMD" "$LIMIT" "${SACC}.faster.fasta"
}

#------------------------------------------------------------------------------------------
#    SPLIT FILES
#------------------------------------------------------------------------------------------

function test_split_files_fastq {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT FILES / FASTQ for ${SACC}"
    #run fasterq-dump
    CMD="$TOOL $1 --split-files -pf"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"
    CMD="$TOOL $1 --split-files -pf -o ${SACC}.faster.fastq"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"
}

function test_split_files_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT FILES / FASTA for ${SACC}"
    #run fasterq-dump
    CMD="$TOOL $1 --split-files --fasta -pf"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"
    CMD="$TOOL $1 --split-files --fasta -pf -o ${SACC}.faster.fasta"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"
}

#------------------------------------------------------------------------------------------
#    SPLIT-3
#------------------------------------------------------------------------------------------

function test_split_3_fastq {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT 3 / FASTQ for ${SACC}"
    #run fasterq-dump
    CMD="$TOOL $1 --split-3 -pf"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"
    CMD="$TOOL $1 --split-3 -pf -o ${SACC}.faster.fastq"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"
}

function test_split_3_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT 3 / FASTA for ${SACC}"
    #run fasterq-dump
    CMD="$TOOL $1 --split-3 --fasta -pf"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"
    CMD="$TOOL $1 --split-3 --fasta -pf -o ${SACC}.faster.fasta"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"
}

#------------------------------------------------------------------------------------------
#    FASTA unsorted
#------------------------------------------------------------------------------------------
function test_unsorted_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: UNSORTED ( SPLIT-SPOT ) / FASTA for ${SACC}"
    #run fasterq-dump
    CMD="$TOOL $1 --fasta-unsorted -pf"
    run_limited "$CMD" "$LIMIT" "{$SACC}.fasta"
    CMD="$TOOL $1 --fasta-unsorted -pf -o ${SACC}.faster.fasta"
    run_limited "$CMD" "$LIMIT" "${SACC}.fasta"
}

#------------------------------------------------------------------------------------------
#    FASTA unsorted + only-aligned + only-unaligned
#------------------------------------------------------------------------------------------
function test_unsorted_fasta_parts {
    SACC=`basename $1`
    echo "" && echo "testing: UNSORTED FASTA ( only aligned/unaligned ) for ${SACC}"

    #run fasterq-dump twice ( only-aligned and ony-unaligned )
    CMD="$TOOL $1 --fasta-unsorted -pf --only-aligned"
    run_limited "$CMD" "$LIMIT" "${SACC}.fasta"
    CMD="$TOOL $1 --fasta-unsorted -pf -o ${SACC}.only_aligned --only-aligned"
    run_limited "$CMD" "$LIMIT" "${SACC}.*"

    CMD="$TOOL $1 --fasta-unsorted -pf --only-unaligned"
    run_limited "$CMD" "$LIMIT" "${SACC}.fasta"
    CMD="$TOOL $1 --fasta-unsorted -pf -o ${SACC}.only_unaligned --only-unaligned"
    run_limited "$CMD" "$LIMIT" "${SACC}*"
}

#------------------------------------------------------------------------------------------

TOOL_LOC=`which $TOOL`
if [ ! -f $TOOL_LOC ]; then
    echo "cannot find $TOOL! aborting"
    exit 3
fi

echo "$TOOL is located at $TOOL_LOC"

$TOOL -V
retcode=$?
if [[ $retcode -ne 0 ]]; then
    echo "cannot execute $TOOL! aborting"
    exit 3
fi

ACC1="SRR000001"
ACC2="SRR341578"

if [ -f "get_acc.sh" ]; then
    ACC1=`./get_acc.sh $ACC1`
    ACC2=`./get_acc.sh $ACC2`
fi

ACCESSIONS="$ACC1 $ACC2"

for acc in $ACCESSIONS
do
    #prefetch -p $acc

    test_whole_spot_fastq $acc
    test_whole_spot_fasta $acc

    test_split_spot_fastq $acc
    test_split_spot_fasta $acc

    test_split_files_fastq $acc
    test_split_files_fasta $acc

    test_split_3_fastq $acc
    test_split_3_fasta $acc

    test_unsorted_fasta $acc
    test_unsorted_fasta_parts $acc

    #rm -rf $acc
done

