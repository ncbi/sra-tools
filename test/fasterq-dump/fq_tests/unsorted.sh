#!/bin/bash

VERBOSE=0

#"including common functions:"
. ./cmn.sh

# $1...ACC, $2...SACC
function ref_unsorted_fasta {
	KEY="${2}.unsorted_fasta.reference.md5"
	MD5_REF_VALUE=`get_md5 $KEY`
	if [ -z "$MD5_REF_VALUE" ]; then
        echo_verbose "" && echo_verbose "producing reference-md5-sum for UNSORTED / FASTA for $2"
        #run fastq-dump as reference
        run "$REFTOOL $1 -B --split-spot --skip-technical --fasta 0"
		SORTED="${2}.fasta.sorted"
        ./fasta_2_line.py "${2}.fasta" | LC_COLLATE=POSIX sort > "$SORTED"
        rm -f "${2}.fasta"
        MD5_REF_VALUE=`md5_of_file "$SORTED"`
		set_md5 "$KEY" "$MD5_REF_VALUE"
    fi
}

#------------------------------------------------------------------------------------------
#    FASTA unsorted
#------------------------------------------------------------------------------------------
function test_unsorted_fasta {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: UNSORTED ( SPLIT-SPOT ) / FASTA for ${SACC}"
	#get key from db ( it is the same key for whole-file and parts )
	ref_unsorted_fasta "$1" "$SACC"
    #run fasterq-dump
    DEFLINE="--seq-defline '>\$ac.\$si \$sn length=\$rl'"
    gen_options "--fasta-unsorted -f $DEFLINE"
    FASTA_OUT="${SACC}.faster.fasta"
    run "$TOOL $1 $OPTIONS -o $FASTA_OUT 2>/dev/null"
    if [ -f "$FASTA_OUT" ]; then
		SORTED="${FASTA_OUT}.sorted"
    	./fasta_2_line.py "$FASTA_OUT" | LC_COLLATE=POSIX sort > "$SORTED"
   		rm "$FASTA_OUT"
		MD5_VALUE=`md5_of_file "$SORTED"`
		compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "UNSORTED-FASTA.$SACC"
    else
    	echo "$FASTA_OUT is missing"
    fi
}

#------------------------------------------------------------------------------------------
#    FASTA unsorted + only-aligned + only-unaligned
#------------------------------------------------------------------------------------------
function test_unsorted_fasta_parts {
    SACC=`basename $1`
    echo_verbose "" && echo_verbose "testing: UNSORTED FASTA ( only aligned/unaligned ) for ${SACC}"
	#get key from db ( it is the same key for whole-file and parts )
	ref_unsorted_fasta "$1" "$SACC"
    #run fasterq-dump twice ( only-aligned and ony-unaligned )
    DEFLINE="--seq-defline '>\$ac.\$si \$sn length=\$rl'"
    gen_options "--fasta-unsorted -f $DEFLINE"
    run "$TOOL $1 $OPTIONS -o ${SACC}.only_aligned --only-aligned 2>/dev/null"
    run "$TOOL $1 $OPTIONS -o ${SACC}.only_unaligned --only-unaligned 2>/dev/null"
	COMBINED="${SACC}.faster.fasta"
    if [ -f "${SACC}.only_aligned" ]; then
    	cat "${SACC}.only_aligned" "${SACC}.only_unaligned" > "$COMBINED"
    else
    	cat "${SACC}.only_unaligned" > "$COMBINED"
    fi
    rm -f "${SACC}.only_aligned" "${SACC}.only_unaligned"
	SORTED="${SACC}.faster.fasta.sorted"
    ./fasta_2_line.py "$COMBINED" | LC_COLLATE=POSIX sort > "$SORTED"
    rm "$COMBINED"
	MD5_VALUE=`md5_of_file "$SORTED"`
	compare_md5 "$MD5_REF_VALUE" "$MD5_VALUE" "UNSORTED-FASTA-PARTS.$SACC"
}

ACC="$1"
echo "	- FASTA unsorted: $ACC"
check_tools
test_unsorted_fasta $ACC
test_unsorted_fasta_parts $ACC
