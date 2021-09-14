ACC1="SRR000001"
ACC2="SRR341578"

function compare_and_delete {
    if [ -f "$1" ]; then
        if [ -f "$2" ]; then    
            diff $1 $2 -sb
            rm $1 $2
        fi
    fi
}

#------------------------------------------------------------------------------------------

function test_whole_spot_fastq {
    echo "" && echo "testing: whole spot FASTQ for $1"
    fastq-dump $1 1>/dev/null 2>/dev/null
    fasterq-dump $1 --include-technical --concatenate-reads -o $1.faster.fastq 1>/dev/null 2>/dev/null
    compare_and_delete $1.fastq $1.faster.fastq
}

function test_whole_spot_fasta {
    echo "" && echo "testing: whole spot FASTA for $1"
    fastq-dump $1 --fasta 0 1>/dev/null 2>/dev/null
    fasterq-dump $1 --include-technical --concatenate-reads --fasta -o $1.faster.fasta 1>/dev/null 2>/dev/null
    compare_and_delete $1.fasta $1.faster.fasta
}

function test_whole_spot {
    test_whole_spot_fastq $1
    test_whole_spot_fasta $1
}

#------------------------------------------------------------------------------------------

function test_split_spot_fastq {
    echo "" && echo "testing: split spot FASTQ for $1"
    fastq-dump $1 --split-spot --skip-technical 1>/dev/null 2>/dev/null
    fasterq-dump $1 --split-spot -o $1.faster.fastq 1>/dev/null 2>/dev/null
    compare_and_delete $1.fastq $1.faster.fastq
}

function test_split_spot_fasta {
    echo "" && echo "testing: split spot FASTA for $1"
    fastq-dump $1 --split-spot --skip-technical --fasta 0 1>/dev/null 2>/dev/null
    fasterq-dump $1 --split-spot --fasta -o $1.faster.fasta 1>/dev/null 2>/dev/null
    compare_and_delete $1.fasta $1.faster.fasta
}

function test_split_spot {
    test_split_spot_fastq $1
    test_split_spot_fasta $1
}

#------------------------------------------------------------------------------------------

function test_split_files_fastq {
    echo "" && echo "testing: split files FASTQ for $1"
    fastq-dump $1 --split-files --skip-technical 1>/dev/null 2>/dev/null
    fasterq-dump $1 --split-files -o $1.fasterq.fastq 1>/dev/null 2>/dev/null
    for num in {1..4}
    do
        compare_and_delete "$1_$num.fastq" "$1.fasterq_$num.fastq"
    done
}

function test_split_files_fasta {
    echo "" && echo "testing: split files FASTA for $1"
    fastq-dump $1 --split-files --skip-technical --fasta 0 1>/dev/null 2>/dev/null
    fasterq-dump $1 --split-files --fasta -o $1.fasterq.fasta 1>/dev/null 2>/dev/null
    for num in {1..4}
    do
        compare_and_delete "$1_$num.fasta" "$1.fasterq_$num.fasta"
    done
}

function test_split_files {
    test_split_files_fastq $1
    test_split_files_fasta $1
}
#------------------------------------------------------------------------------------------

function test_split_3_fastq {
    echo "" && echo "testing: split 3 FASTQ for $1"
    fastq-dump $1 --split-3 --skip-technical 1>/dev/null 2>/dev/null
    fasterq-dump $1 --split-3 -o $1.fasterq.fastq 1>/dev/null 2>/dev/null
    for num in {1..4}
    do
        compare_and_delete "$1_$num.fastq" "$1.fasterq_$num.fastq"
    done
}

function test_split_3_fasta {
    echo "" && echo "testing: split 3 FASTA for $1"
    fastq-dump $1 --split-3 --skip-technical --fasta 0 1>/dev/null 2>/dev/null
    fasterq-dump $1 --split-3 --fasta -o $1.fasterq.fasta 1>/dev/null 2>/dev/null
    for num in {1..4}
    do
        compare_and_delete "$1_$num.fasta" "$1.fasterq_$num.fasta"
    done
}

function test_split_3 {
    test_split_3_fastq $1
    test_split_3_fasta $1
}

#------------------------------------------------------------------------------------------

function test_accession {
    prefetch $1
    test_whole_spot $1
    test_split_spot $1
    test_split_files $1
    test_split_3 $1
    echo ""
}

for acc in $ACC2
do
    test_accession $acc
done
