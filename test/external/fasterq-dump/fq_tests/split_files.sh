#!/bin/bash

#"including common functions:"
. ./cmn.sh

#------------------------------------------------------------------------------------------
#    SPLIT FILES
#------------------------------------------------------------------------------------------

function test_split_files_fastq {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: SPLIT FILES / FASTQ for ${SACC}"
	#test if any of the 4 ref-files do NOT have a stored md5-ref-value
    i=0
    for num in {1..4}
    do
        KEY="${SACC}.split_files.fastq_${num}.reference.md5"
		if [ -z `get_md5 $KEY` ]; then
            ((i++))
        fi
    done
	#we do have to re-do them:
    if [[ "$i" -gt 0 ]]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for SPLIT FILES / FASTQ for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 -B --split-files --skip-technical"
        for num in {1..4}
        do
        	MD5SRC="${SACC}_${num}.fastq"
			KEY="${SACC}.split_files.fastq_${num}.reference.md5"
			set_md5 "$KEY" `md5_of_file "$MD5SRC"`
        done
    fi
    #run fasterq-dump
    gen_options "--split-files -f"
	OUTFILE="${SACC}.faster.fastq"
    run "$TOOL $1 $OPTIONS -o $OUTFILE 2>/dev/null"
	#in case a not-numbered output-file has been produced
	if [ -f "$OUTFILE" ]; then
		rm "$OUTFILE"
	fi
    for num in {1..4}
    do
    	MD5SRC="${SACC}.faster_${num}.fastq"
    	KEY="${SACC}.split_files.fastq_${num}.reference.md5"
		MD5_REF_VALUE=`get_md5 $KEY`
        if [ -f "$MD5SRC" ]; then
			MD5_VALUE=`md5_of_file "$MD5SRC"`
			compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "SPLIT-FILES.FASTQ.${num}.${SACC}"
		else
			if [[ "$MD5_REF_VALUE" != "-" ]]; then
            	echo "${SACC}-SPLIT-FILES-FASTQ : fastq-dump produced split-file #${num}, but fasterq-dump did not"
            	exit 3
			fi
        fi
    done
}

function test_split_files_fasta {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: SPLIT FILES / FASTA for ${SACC}"
    i=0
	#test if any of the 4 ref-files do NOT have a stored md5-ref-value
    for num in {1..4}
    do
        KEY="${SACC}.split_files.fasta_${num}.reference.md5"
        if [ -z `get_md5 $KEY` ]; then
            ((i++))
        fi
    done
	#we do have to re-do them:
    if [[ "$i" -gt 0 ]]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for SPLIT FILES / FASTA for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 -B --split-files --skip-technical --fasta 0"
        for num in {1..4}
        do
        	MD5SRC="${SACC}_${num}.fasta"
            KEY="${SACC}.split_files.fasta_${num}.reference.md5"
			set_md5 "$KEY" `md5_of_file "$MD5SRC"`
        done
    fi
    #run fasterq-dump
    gen_options "--split-files --fasta -f"
	OUTFILE="${SACC}.faster.fasta"
    run "$TOOL $1 $OPTIONS -o $OUTFILE 2>/dev/null"
	if [ -f "$OUTFILE" ]; then
		rm "$OUTFILE"
	fi
    for num in {1..4}
    do
    	MD5SRC="${SACC}.faster_${num}.fasta"
    	KEY="${SACC}.split_files.fasta_${num}.reference.md5"
		MD5_REF_VALUE=`get_md5 $KEY`
        if [ -f "$MD5SRC" ]; then
			MD5_VALUE=`md5_of_file "$MD5SRC"`
			compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "SPLIT-FILES.FASTA.${num}.${SACC}"
		else
			if [[ "$MD5_REF_VALUE" != "-" ]]; then
            	echo "${SACC}-SPLIT-FILES-FASTA : fastq-dump produced split-file #${num}, but fasterq-dump did not"
            	exit 3
			fi
        fi
    done
}

ACC="$1"

echo "	- SPLIT_FILES: $ACC"
check_tools
VERBOSE=0
test_split_files_fastq $ACC
test_split_files_fasta $ACC
