#!/bin/bash

#"including common functions:"
. ./cmn.sh

#------------------------------------------------------------------------------------------
#    SPLIT-3
#------------------------------------------------------------------------------------------

function test_split_3_fastq {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: SPLIT 3 / FASTQ for ${SACC}"
    i=0
    for num in {1..4}
    do
        KEY="${SACC}.split_3.fastq_${num}.reference.md5"
        if [ -z `get_md5 $KEY` ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for SPLIT-3 / FASTQ for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 -B --split-3 --skip-technical"
        for num in {1..4}
        do
        	MD5SRC="${SACC}_${num}.fastq"
            KEY="${SACC}.split_3.fastq_${num}.reference.md5"
			set_md5 "$KEY" `md5_of_file "$MD5SRC"`
        done
        MD5SRC="${SACC}.fastq"
        KEY="${SACC}.split_3.fastq.reference.md5"
		set_md5 "$KEY" `md5_of_file "$MD5SRC"`
    fi
    #run fasterq-dump
    gen_options "--split-3 -f"
    run "$TOOL $1 $OPTIONS -o ${SACC}.faster.fastq 2>/dev/null"
    for num in {1..4}
    do
    	MD5SRC="${SACC}.faster_${num}.fastq"
        KEY="${SACC}.split_3.fastq_${num}.reference.md5"
		MD5_REF_VALUE=`get_md5 $KEY`
        if [ -f "$MD5SRC" ]; then
			MD5_VALUE=`md5_of_file $MD5SRC`
            compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "SPLIT-3.FASTQ.${num}.${SACC}"
		else
			if [[ "$MD5_REF_VALUE" != "-" ]]; then
            	echo "${SACC}-SPLIT3-FASTQ : fastq-dump produced split-file #${num}, but fasterq-dump did not"
            	exit 3
			fi
        fi
    done
    MD5SRC="${SACC}.faster.fastq"
    KEY="${SACC}.split_3.fastq.reference.md5"
	MD5_REF_VALUE=`get_md5 $KEY`
    if [ -f "$MD5SRC" ]; then
		MD5_VALUE=`md5_of_file $MD5SRC`
        compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "SPLIT-3.FASTQ.${SACC}"
	else
		if [[ "$MD5_REF_VALUE" != "-" ]]; then
            echo "${SACC}-SPLIT3-FASTQ : fastq-dump produced split-file, but fasterq-dump did not"
            exit 3
		fi
    fi
}

function test_split_3_fasta {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: SPLIT 3 / FASTA for ${SACC}"
    i=0
    for num in {1..4}
    do
        KEY="${SACC}.split_3.fasta_${num}.reference.md5"
        if [ -z `get_md5 $KEY` ]; then
            ((i++))
        fi
    done
    if [[ "$i" -gt 0 ]]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for SPLIT-3 / FASTA for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 -B --split-3 --skip-technical --fasta 0"
        for num in {1..4}
        do
        	MD5SRC="${SACC}_${num}.fasta"
            KEY="${SACC}.split_3.fasta_${num}.reference.md5"
			set_md5 "$KEY" `md5_of_file "$MD5SRC"`
        done
        MD5SRC="${SACC}.fasta"
        KEY="${SACC}.split_3.fasta.reference.md5"
		set_md5 "$KEY" `md5_of_file "$MD5SRC"`
    fi
    #run fasterq-dump
    gen_options "--split-3 --fasta -f"
    run "$TOOL $1 $OPTIONS -o ${SACC}.faster.fasta 2>/dev/null"
    for num in {1..4}
    do
    	MD5SRC="${SACC}.faster_${num}.fasta"
		KEY="${SACC}.split_3.fasta_${num}.reference.md5"
		MD5_REF_VALUE=`get_md5 $KEY`
        if [ -f "$MD5SRC" ]; then
			MD5_VALUE=`md5_of_file $MD5SRC`
            compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "SPLIT-3.FASTA.${num}.${SACC}"
		else
			if [[ "$MD5_REF_VALUE" != "-" ]]; then
            	echo "${SACC}-SPLIT3-FASTA : fastq-dump produced split-file #${num}, but fasterq-dump did not"
            	exit 3
			fi
        fi
    done
    MD5SRC="${SACC}.faster.fasta"
	KEY="${SACC}.split_3.fasta.reference.md5"
	MD5_REF_VALUE=`get_md5 $KEY`
    if [ -f "$MD5SRC" ]; then
		MD5_VALUE=`md5_of_file $MD5SRC`
        compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "SPLIT-3.FASTA.${SACC}"
	else
		if [[ "$MD5_REF_VALUE" != "-" ]]; then
            echo "${SACC}-SPLIT3-FASTA : fastq-dump produced split-file, but fasterq-dump did not"
            exit 3
		fi
	fi
}

ACC="$1"

echo "	- SPLIT_3: $ACC"
check_tools
VERBOSE=0
test_split_3_fastq $ACC
test_split_3_fasta $ACC
