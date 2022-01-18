#------------------------------------------------------------------------------------------
#
#   compare default-output of fasterq-dump vs fastq-dump in different modes
#   on a flat table and a cSRA accession
#
#------------------------------------------------------------------------------------------

TOOL="fasterq-dump"
REFTOOL="fastq-dump"
MD5_LOC="./ref_md5"

function compare {
    if [ -f "$1" ]; then
        if [ -f "$2" ]; then    
            diff "$1" "$2" -sb
            retcode=$?
            if [[ $retcode -ne 0 ]]; then
                echo "difference in md5-sum! aborting"
                exit 3
            fi
        fi
    fi
}

#   $1 ... PRODUCT
#   $2 ... MD5REFERENCE
function produce_md5_and_compare {
    md5sum "$1" | cut -d ' ' -f 1 > TEMP.MD5
    compare "$2" TEMP.MD5
    rm -f "$1" TEMP.MD5
}

function run {
    echo "$1"
    eval "time $1"
}

#------------------------------------------------------------------------------------------
#    WHOLE SPOT
#------------------------------------------------------------------------------------------

function test_whole_spot_fastq {
    SACC=`basename $1`
    echo "" && echo "testing: WHOLE SPOT / FASTQ for ${SACC}"
    MD5REFERENCE="${MD5_LOC}/${SACC}.whole_spot_fastq.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for WHOLE SPOT / FASTQ for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1"
        md5sum "${SACC}.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "${SACC}.fastq"
    fi
    #run fasterq-dump
    run "$TOOL $1 --include-technical --concatenate-reads -pf -o ${SACC}.faster.fastq"
    produce_md5_and_compare "${SACC}.faster.fastq" "$MD5REFERENCE"
}

function test_whole_spot_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: WHOLE SPOT / FASTA for ${SACC}"
    MD5REFERENCE="${MD5_LOC}/${SACC}.whole_spot_fasta.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for WHOLE SPOT / FASTA for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 --fasta 0"
        md5sum "${SACC}.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "${SACC}.fasta"
    fi
    #run fasterq-dump
    run "$TOOL $1 --include-technical --concatenate-reads --fasta -pf -o ${SACC}.faster.fasta"
    produce_md5_and_compare "${SACC}.faster.fasta" "$MD5REFERENCE"
}

#------------------------------------------------------------------------------------------
#    SPLIT SPOT
#------------------------------------------------------------------------------------------

function test_split_spot_fastq {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT SPOT / FASTQ for ${SACC}"
    MD5REFERENCE="${MD5_LOC}/${SACC}.split_spot_fastq.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for SPLIT SPOT / FASTQ for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 --split-spot --skip-technical"
        md5sum "${SACC}.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "${SACC}.fastq"
    fi
    #run fasterq-dump
    run "$TOOL $1 --split-spot -pf -o ${SACC}.faster.fastq"
    produce_md5_and_compare "${SACC}.faster.fastq" "$MD5REFERENCE"
}

function test_split_spot_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT SPOT / FASTA for ${SACC}"
    MD5REFERENCE="${MD5_LOC}/${SACC}.split_spot_fasta.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for SPLIT SPOT / FASTA for ${SACC}"
        #run fasterq-dump as reference
        run "$REFTOOL $1 --split-spot --skip-technical --fasta 0"
        md5sum "${SACC}.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "${SACC}.fasta"
    fi
    #run fasterq-dump
    run "$TOOL $1 --split-spot --fasta -pf -o ${SACC}.faster.fasta"
    produce_md5_and_compare "${SACC}.faster.fasta" "$MD5REFERENCE"
}

#------------------------------------------------------------------------------------------
#    SPLIT FILES
#------------------------------------------------------------------------------------------

function test_split_files_fastq {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT FILES / FASTQ for ${SACC}"
    i=0
    for num in {1..4}
    do
        MD5REFERENCE="${MD5_LOC}/${SACC}.split_files.fastq_${num}.reference.md5"
        if [ ! -f "$MD5REFERENCE" ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo "" && echo "producing reference-md5-sum for SPLIT FILES / FASTQ for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 --split-files --skip-technical"
        for num in {1..4}
        do
            MD5REFERENCE="${MD5_LOC}/${SACC}.split_files.fastq_${num}.reference.md5"
            md5sum "${SACC}_${num}.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
            rm -f "${SACC}_$num.fastq"
        done
    fi
    #run fasterq-dump
    run "$TOOL $1 --split-files -pf -o ${SACC}.faster.fastq"
    for num in {1..4}
    do
        if [ -f "${SACC}.faster_${num}.fastq" ]; then
            MD5REFERENCE="${MD5_LOC}/${SACC}.split_files.fastq_${num}.reference.md5"
            produce_md5_and_compare "${SACC}.faster_${num}.fastq" "$MD5REFERENCE"
        fi
    done
}

function test_split_files_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT FILES / FASTA for ${SACC}"
    i=0
    for num in {1..4}
    do
        MD5REFERENCE="${MD5_LOC}/${SACC}.split_files.fasta_${num}.reference.md5"
        if [ ! -f "$MD5REFERENCE" ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo "" && echo "producing reference-md5-sum for SPLIT FILES / FASTA for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 --split-files --skip-technical --fasta 0"
        for num in {1..4}
        do
            MD5REFERENCE="${MD5_LOC}/${SACC}.split_files.fasta_${num}.reference.md5"
            md5sum "${SACC}_${num}.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
            rm -f "${SACC}_${num}.fasta"
        done
    fi
    #run fasterq-dump
    run "$TOOL $1 --split-files --fasta -pf -o ${SACC}.faster.fasta"
    for num in {1..4}
    do
        if [ -f "${SACC}.faster_${num}.fasta" ]; then
            MD5REFERENCE="${MD5_LOC}/${SACC}.split_files.fasta_${num}.reference.md5"
            produce_md5_and_compare "${SACC}.faster_${num}.fasta" "$MD5REFERENCE"
        fi
    done
}

#------------------------------------------------------------------------------------------
#    SPLIT-3
#------------------------------------------------------------------------------------------

function test_split_3_fastq {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT 3 / FASTQ for ${SACC}"
    i=0
    for num in {1..4}
    do
        MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fastq_${num}.reference.md5"
        if [ ! -f "$MD5REFERENCE" ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo "" && echo "producing reference-md5-sum for SPLIT-3 / FASTQ for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 --split-3 --skip-technical"
        for num in {1..4}
        do
            MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fastq_${num}.reference.md5"
            md5sum "${SACC}_${num}.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
            rm -f "${SACC}_${num}.fastq"
        done
        MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fastq.reference.md5"
        md5sum "${SACC}.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "${SACC}.fastq"
    fi
    #run fasterq-dump
    run "$TOOL $1 --split-3 -pf -o ${SACC}.faster.fastq"
    for num in {1..4}
    do
        if [ -f "${SACC}.faster_${num}.fastq" ]; then
            MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fastq_${num}.reference.md5"
            produce_md5_and_compare "${SACC}.faster_${num}.fastq" "$MD5REFERENCE"
        fi
    done
    if [ -f "${SACC}.faster.fastq" ]; then
        MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fastq.reference.md5"
        produce_md5_and_compare "${SACC}.faster.fastq" "$MD5REFERENCE"
    fi
}

function test_split_3_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: SPLIT 3 / FASTA for ${SACC}"
    i=0
    for num in {1..4}
    do
        MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fasta_${num}.reference.md5"
        if [ ! -f "$MD5REFERENCE" ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo "" && echo "producing reference-md5-sum for SPLIT-3 / FASTA for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 --split-3 --skip-technical --fasta 0"
        for num in {1..4}
        do
            MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fasta_${num}.reference.md5"
            md5sum "${SACC}_${num}.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
            rm -f "${SACC}_${num}.fasta"
        done
        MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fasta.reference.md5"
        md5sum "${SACC}.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "${SACC}.fasta"
    fi
    #run fasterq-dump
    run "$TOOL $1 --split-3 --fasta -pf -o ${SACC}.faster.fasta"
    for num in {1..4}
    do
        if [ -f "${SACC}.faster_${num}.fasta" ]; then
            MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fasta_${num}.reference.md5"
            produce_md5_and_compare "${SACC}.faster_${num}.fasta" "$MD5REFERENCE"
        fi
    done
    if [ -f "${SACC}.faster.fasta" ]; then
        MD5REFERENCE="${MD5_LOC}/${SACC}.split_3.fasta.reference.md5"
        produce_md5_and_compare "${SACC}.faster.fasta" "$MD5REFERENCE"
    fi
}

#------------------------------------------------------------------------------------------
#    FASTA unsorted
#------------------------------------------------------------------------------------------
function test_unsorted_fasta {
    SACC=`basename $1`
    echo "" && echo "testing: UNSORTED ( SPLIT-SPOT ) / FASTA for ${SACC}"
    MD5REFERENCE="${MD5_LOC}/${SACC}.unsorted_fasta.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for UNSORTED / FASTA for $SACC"
        #run fastq-dump as reference
        run "$REFTOOL $1 --split-spot --skip-technical --fasta 0"
        ./fasta_2_line.py "${SACC}.fasta" | sort > "${SACC}.fasta.sorted"
        rm -f "${SACC}.fasta"
        md5sum "${SACC}.fasta.sorted" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "${SACC}.fasta.sorted"
    fi
    #run fasterq-dump
    DEFLINE="--seq-defline '>\$ac.\$si \$sn length=\$rl'"
    run "$TOOL $1 --fasta-unsorted -pf -o ${SACC}.faster.fasta $DEFLINE"
    ./fasta_2_line.py "${SACC}.faster.fasta" | sort > "${SACC}.faster.fasta.sorted"
    rm "${SACC}.faster.fasta"
    produce_md5_and_compare "${SACC}.faster.fasta.sorted" "$MD5REFERENCE"
}

#------------------------------------------------------------------------------------------
#    FASTA unsorted + only-aligned + only-unaligned
#------------------------------------------------------------------------------------------
function test_unsorted_fasta_parts {
    SACC=`basename $1`
    echo "" && echo "testing: UNSORTED FASTA ( only aligned/unaligned ) for ${SACC}"
    MD5REFERENCE="${MD5_LOC}/${SACC}.unsorted_fasta.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for UNSORTED / FASTA for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 --split-spot --skip-technical --fasta 0"
        ./fasta_2_line.py "${SACC}.fasta" | sort > "${SACC}.fasta.sorted"
        rm -f "${SACC}.fasta"
        md5sum "${SACC}.fasta.sorted" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "${SACC}.fasta.sorted"
    fi
    #run fasterq-dump twice ( only-aligned and ony-unaligned )
    DEFLINE="--seq-defline '>\$ac.\$si \$sn length=\$rl'"
    run "$TOOL $1 --fasta-unsorted -pf -o ${SACC}.only_aligned --only-aligned $DEFLINE"
    run "$TOOL $1 --fasta-unsorted -pf -o ${SACC}.only_unaligned --only-unaligned $DEFLINE"
    cat "${SACC}.only_aligned" "${SACC}.only_unaligned" > "${SACC}.faster.fasta"
    rm -f "${SACC}.only_aligned" "${SACC}.only_unaligned"
    ./fasta_2_line.py "${SACC}.faster.fasta" | sort > "${SACC}.faster.fasta.sorted"
    rm "${SACC}.faster.fasta"
    produce_md5_and_compare "${SACC}.faster.fasta.sorted" "$MD5REFERENCE"
}

#------------------------------------------------------------------------------------------

TOOL_LOC=`which $TOOL`
if [ ! -f $TOOL_LOC ]; then
    echo "cannot find $TOOL! aborting"
    exit 3
fi

echo "$TOOL is located at $TOOL_LOC"

TOOL_LOC=`which $REFTOOL`
if [ ! -f $TOOL_LOC ]; then
    echo "cannot find $REFTOOL! aborting"
    exit 3
fi
echo "$REFTOOL is located at $TOOL_LOC"

$TOOL -V
retcode=$?
if [[ $retcode -ne 0 ]]; then
    echo "cannot execute $TOOL! aborting"
    exit 3
fi

$REFTOOL -V
retcode=$?
if [[ $retcode -ne 0 ]]; then
    echo "cannot execute $REFTOOL! aborting"
    exit 3
fi

#just in case: it does not exist and we have to create the reference-md5-files
mkdir -p $MD5_LOC

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

