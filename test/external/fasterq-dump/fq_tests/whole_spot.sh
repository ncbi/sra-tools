#!/bin/bash

#"including common functions:"
. ./cmn.sh

#------------------------------------------------------------------------------------------
#    WHOLE SPOT
#------------------------------------------------------------------------------------------

function test_whole_spot_fastq {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: WHOLE SPOT / FASTQ for ${SACC}"
    KEY="${SACC}.whole_spot_fastq.reference.md5"
	MD5_REF_VALUE=`get_md5 $KEY`
	if [ -z "$MD5_REF_VALUE" ]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for WHOLE SPOT / FASTQ for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 -B"
		MD5_REF_VALUE=`md5_of_file "${SACC}.fastq"`
		set_md5 "$KEY" "$MD5_REF_VALUE"
    fi
    #run fasterq-dump
    gen_options "--include-technical --concatenate-reads -f"
	FASTERQ_FILE="${SACC}.faster.fastq"
    run "$TOOL $1 $OPTIONS -o ${FASTERQ_FILE} 2>/dev/null"
    MD5_VALUE=`md5_of_file "$FASTERQ_FILE"`
	compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "WHOLE-SPOT.FASTQ.$SACC"
}

function test_whole_spot_fasta {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: WHOLE SPOT / FASTA for ${SACC}"
    KEY="${SACC}.whole_spot_fasta.reference.md5"
	MD5_REF_VALUE=`get_md5 $KEY`
	if [ -z "$MD5_REF_VALUE" ]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for WHOLE SPOT / FASTA for ${SACC}"
        #run fastq-dump as reference
        run "$REFTOOL $1 -B --fasta 0"
		MD5_REF_VALUE=`md5_of_file "${SACC}.fasta"`
		set_md5 "$KEY" "$MD5_REF_VALUE"
    fi
    #run fasterq-dump
    gen_options "--include-technical --concatenate-reads --fasta -f"
	FASTERQ_FILE="${SACC}.faster.fasta"
    run "$TOOL $1 $OPTIONS -o ${FASTERQ_FILE} 2>/dev/null"
    MD5_VALUE=`md5_of_file "$FASTERQ_FILE"`
	compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "WHOLE-SPOT.FASTA.$SACC"
}

ACC="$1"

echo "	- WHOLE_SPOT: $ACC"
check_tools
VERBOSE=0
test_whole_spot_fastq $ACC
test_whole_spot_fasta $ACC
