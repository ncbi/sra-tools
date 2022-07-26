#!/bin/bash

#"including common functions:"
. ./cmn.sh

#------------------------------------------------------------------------------------------
#    SPLIT SPOT
#------------------------------------------------------------------------------------------

function test_split_spot_fastq {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: SPLIT SPOT / FASTQ for ${SACC}"
    KEY="${SACC}.split_spot_fastq.reference.md5"
	MD5_REF_VALUE=`get_md5 $KEY`
	if [ -z "$MD5_REF_VALUE" ]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for SPLIT SPOT / FASTQ for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 --split-spot --skip-technical -B"
		MD5_REF_VALUE=`md5_of_file "${SACC}.fastq"`
		set_md5 "$KEY" "$MD5_REF_VALUE"
    fi
    #run fasterq-dump
    gen_options "--split-spot -f"
	FASTERQ_FILE="${SACC}.faster.fastq"
    run "$TOOL $1 $OPTIONS -o $FASTERQ_FILE 2>/dev/null"
    MD5_VALUE=`md5_of_file "$FASTERQ_FILE"`
	compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "SPLIT-SPOT.FASTQ.$SACC"
}

function test_split_spot_fasta {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: SPLIT SPOT / FASTA for ${SACC}"
    KEY="${SACC}.split_spot_fasta.reference.md5"
	MD5_REF_VALUE=`get_md5 $KEY`
	if [ -z "$MD5_REF_VALUE" ]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for SPLIT SPOT / FASTA for ${SACC}"
        #run fasterq-dump as reference
        run "$REFTOOL $1 --split-spot -B --skip-technical --fasta 0"
		MD5_REF_VALUE=`md5_of_file "${SACC}.fasta"`
		set_md5 "$KEY" "$MD5_REF_VALUE"
    fi
    #run fasterq-dump
    gen_options "--split-spot --fasta -f"
	FASTERQ_FILE="${SACC}.faster.fasta"
    run "$TOOL $1 $OPTIONS -o $FASTERQ_FILE 2>/dev/null"
    MD5_VALUE=`md5_of_file "$FASTERQ_FILE"`
	compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "SPLIT-SPOT.FASTA.$SACC"
}

ACC="$1"

echo "	- SPLIT_SPOT: $ACC"
check_tools
VERBOSE=0
test_split_spot_fastq $ACC
test_split_spot_fasta $ACC
