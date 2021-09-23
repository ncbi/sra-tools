
TOOL="fasterq-dump"
REFTOOL="fastq-dump"

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

#------------------------------------------------------------------------------------------
#    WHOLE SPOT
#------------------------------------------------------------------------------------------

function test_whole_spot_fastq {
    echo "" && echo "testing: WHOLE SPOT / FASTQ for $1"
    MD5REFERENCE="./ref_md5/$1.whole_spot_fastq.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for WHOLE SPOT / FASTQ for $1"
        time $REFTOOL $1
        md5sum "$1.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "$1.fastq"
    fi
    time "$TOOL" "$1" --include-technical --concatenate-reads -pf -o "$1.faster.fastq"
    produce_md5_and_compare "$1.faster.fastq" "$MD5REFERENCE"
}

function test_whole_spot_fasta {
    echo "" && echo "testing: WHOLE SPOT / FASTA for $1"
    MD5REFERENCE="./ref_md5/$1.whole_spot_fasta.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for WHOLE SPOT / FASTA for $1"
        time $REFTOOL --fasta 0 $1
        md5sum "$1.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "$1.fasta"
    fi
    time "$TOOL" "$1" --include-technical --concatenate-reads --fasta -pf -o "$1.faster.fasta"
    produce_md5_and_compare "$1.faster.fasta" "$MD5REFERENCE"
}

#------------------------------------------------------------------------------------------
#    SPLIT SPOT
#------------------------------------------------------------------------------------------

function test_split_spot_fastq {
    echo "" && echo "testing: SPLIT SPOT / FASTQ for $1"
    MD5REFERENCE="./ref_md5/$1.split_spot_fastq.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for SPLIT SPOT / FASTQ for $1"
        time $REFTOOL "$1" --split-spot --skip-technical
        md5sum "$1.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "$1.fastq"
    fi
    time "$TOOL" "$1" --split-spot -pf -o "$1.faster.fastq"
    produce_md5_and_compare "$1.faster.fastq" "$MD5REFERENCE"
}

function test_split_spot_fasta {
    echo "" && echo "testing: SPLIT SPOT / FASTA for $1"
    MD5REFERENCE="./ref_md5/$1.split_spot_fasta.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for SPLIT SPOT / FASTA for $1"
        time $REFTOOL "$1" --split-spot --skip-technical --fasta 0
        md5sum "$1.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "$1.fasta"
    fi
    time "$TOOL" "$1" --split-spot --fasta -pf -o "$1.faster.fasta"
    produce_md5_and_compare "$1.faster.fasta" "$MD5REFERENCE"
}

#------------------------------------------------------------------------------------------
#    SPLIT FILES
#------------------------------------------------------------------------------------------

function test_split_files_fastq {
    echo "" && echo "testing: SPLIT FILES / FASTQ for $1"
    i=0
    for num in {1..4}
    do
        MD5REFERENCE="./ref_md5/$1.split_files.fastq_$num.reference.md5"
        if [ ! -f "$MD5REFERENCE" ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo "" && echo "producing reference-md5-sum for SPLIT FILES / FASTQ for $1"
        time $REFTOOL "$1" --split-files --skip-technical
        for num in {1..4}
        do
            MD5REFERENCE="./ref_md5/$1.split_files.fastq_$num.reference.md5"
            md5sum "$1_$num.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
            rm -f "$1_$num.fastq"
        done
    fi
    time "$TOOL" "$1" --split-files -pf -o $1.faster.fastq
    for num in {1..4}
    do
        if [ -f "$1.faster_$num.fastq" ]; then
            MD5REFERENCE="./ref_md5/$1.split_files.fastq_$num.reference.md5"
            produce_md5_and_compare "$1.faster_$num.fastq" "$MD5REFERENCE"
        fi
    done
}

function test_split_files_fasta {
    echo "" && echo "testing: SPLIT FILES / FASTA for $1"
    i=0
    for num in {1..4}
    do
        MD5REFERENCE="./ref_md5/$1.split_files.fasta_$num.reference.md5"
        if [ ! -f "$MD5REFERENCE" ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo "" && echo "producing reference-md5-sum for SPLIT FILES / FASTA for $1"
        time $REFTOOL "$1" --split-files --skip-technical --fasta 0
        for num in {1..4}
        do
            MD5REFERENCE="./ref_md5/$1.split_files.fasta_$num.reference.md5"
            md5sum "$1_$num.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
            rm -f "$1_$num.fasta"
        done
    fi
    time "$TOOL" "$1" --split-files --fasta -pf -o $1.faster.fasta
    for num in {1..4}
    do
        if [ -f "$1.faster_$num.fasta" ]; then
            MD5REFERENCE="./ref_md5/$1.split_files.fasta_$num.reference.md5"
            produce_md5_and_compare "$1.faster_$num.fasta" "$MD5REFERENCE"
        fi
    done
}

#------------------------------------------------------------------------------------------
#    SPLIT-3
#------------------------------------------------------------------------------------------

function test_split_3_fastq {
    echo "" && echo "testing: SPLIT 3 / FASTQ for $1"
    i=0
    for num in {1..4}
    do
        MD5REFERENCE="./ref_md5/$1.split_3.fastq_$num.reference.md5"
        if [ ! -f "$MD5REFERENCE" ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo "" && echo "producing reference-md5-sum for SPLIT-3 / FASTQ for $1"
        time $REFTOOL "$1" --split-3 --skip-technical
        for num in {1..4}
        do
            MD5REFERENCE="./ref_md5/$1.split_3.fastq_$num.reference.md5"
            md5sum "$1_$num.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
            rm -f "$1_$num.fastq"
        done
        MD5REFERENCE="./ref_md5/$1.split_3.fastq.reference.md5"
        md5sum "$1.fastq" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "$1.fastq"
    fi
    time "$TOOL" "$1" --split-3 -pf -o $1.faster.fastq
    for num in {1..4}
    do
        if [ -f "$1.faster_$num.fastq" ]; then
            MD5REFERENCE="./ref_md5/$1.split_3.fastq_$num.reference.md5"
            produce_md5_and_compare "$1.faster_$num.fastq" "$MD5REFERENCE"
        fi
    done
    if [ -f "$1.faster.fastq" ]; then
        MD5REFERENCE="./ref_md5/$1.split_3.fastq.reference.md5"
        produce_md5_and_compare "$1.faster.fastq" "$MD5REFERENCE"
    fi
}

function test_split_3_fasta {
    echo "" && echo "testing: SPLIT 3 / FASTA for $1"
    i=0
    for num in {1..4}
    do
        MD5REFERENCE="./ref_md5/$1.split_3.fasta_$num.reference.md5"
        if [ ! -f "$MD5REFERENCE" ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo "" && echo "producing reference-md5-sum for SPLIT-3 / FASTA for $1"
        time $REFTOOL "$1" --split-3 --skip-technical --fasta 0
        for num in {1..4}
        do
            MD5REFERENCE="./ref_md5/$1.split_3.fasta_$num.reference.md5"
            md5sum "$1_$num.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
            rm -f "$1_$num.fasta"
        done
        MD5REFERENCE="./ref_md5/$1.split_3.fasta.reference.md5"
        md5sum "$1.fasta" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "$1.fasta"
    fi
    time "$TOOL" "$1" --split-3 --fasta -pf -o $1.faster.fasta
    for num in {1..4}
    do
        if [ -f "$1.faster_$num.fasta" ]; then
            MD5REFERENCE="./ref_md5/$1.split_3.fasta_$num.reference.md5"
            produce_md5_and_compare "$1.faster_$num.fasta" "$MD5REFERENCE"
        fi
    done
    if [ -f "$1.faster.fasta" ]; then
        MD5REFERENCE="./ref_md5/$1.split_3.fasta.reference.md5"
        produce_md5_and_compare "$1.faster.fasta" "$MD5REFERENCE"
    fi
}

#------------------------------------------------------------------------------------------
#    FASTA unsorted
#------------------------------------------------------------------------------------------
function test_unsorted_fasta {
    echo "" && echo "testing: UNSORTED ( SPLIT-SPOT ) / FASTA for $1"
    MD5REFERENCE="./ref_md5/$1.unsorted_fasta.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for UNSORTED / FASTA for $1"
        time $REFTOOL $1 --split-spot --skip-technical --fasta 0
        ./fasta_2_line.py $1.fasta | sort > $1.fasta.sorted
        rm -f $1.fasta
        md5sum "$1.fasta.sorted" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "$1.fasta.sorted"
    fi
    time "$TOOL" "$1" --fasta-unsorted -pf -o $1.faster.fasta --seq-defline '>$ac.$si $sn length=$rl'
    ./fasta_2_line.py $1.faster.fasta | sort > $1.faster.fasta.sorted
    rm $1.faster.fasta
    produce_md5_and_compare "$1.faster.fasta.sorted" "$MD5REFERENCE"
}

#------------------------------------------------------------------------------------------
#    FASTA unsorted + only-aligned + only-unaligned
#------------------------------------------------------------------------------------------
function test_unsorted_fasta_parts {
    echo "" && echo "testing: UNSORTED FASTA ( only aligned/unaligned ) for $1"
    MD5REFERENCE="./ref_md5/$1.unsorted_fasta.reference.md5"
    if [ ! -f "$MD5REFERENCE" ]; then
        echo "" && echo "producing reference-md5-sum for UNSORTED / FASTA for $1"
        time $REFTOOL $1 --split-spot --skip-technical --fasta 0
        ./fasta_2_line.py $1.fasta | sort > $1.fasta.sorted
        rm -f $1.fasta
        md5sum "$1.fasta.sorted" | cut -d ' ' -f 1 > "$MD5REFERENCE"
        rm -f "$1.fasta.sorted"
    fi
    time "$TOOL" "$1" --fasta-unsorted -pf -o $1.only_aligned --only-aligned --seq-defline '>$ac.$si $sn length=$rl'
    time "$TOOL" "$1" --fasta-unsorted -pf -o $1.only_unaligned --only-unaligned --seq-defline '>$ac.$si $sn length=$rl'
    cat $1.only_aligned $1.only_unaligned > $1.faster.fasta
    rm -f $1.only_aligned $1.only_unaligned
    ./fasta_2_line.py $1.faster.fasta | sort > $1.faster.fasta.sorted
    rm $1.faster.fasta
    produce_md5_and_compare "$1.faster.fasta.sorted" "$MD5REFERENCE"
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
mkdir -p ref_md5

#------------------------------------------------------------------------------------------
ACC1="SRR000001"
ACC2="SRR341578"
#------------------------------------------------------------------------------------------

ACCESSIONS="$ACC1 $ACC2"

for acc in $ACCESSIONS
do
    prefetch -p $acc

    test_whole_spot_fastq $acc
    test_whole_spot_fasta $acc

    test_split_spot_fastq $acc
    test_split_spot_fasta $acc

    test_split_files_fastq $acc
    test_split_files_fasta $acc

    test_split_3_fastq $acc
    test_split_3_fasta $acc

    test_unsorted_fasta $acc
    #test_unsorted_fasta_parts $acc

    rm -rf $acc
done
