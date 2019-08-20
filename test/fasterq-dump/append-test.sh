#!/bin/bash

SCRATCH_DIR="/panfs/traces01/sra_review/scratch"
TEMP_DIR="/dev/shm"

ACC1="SRR341578"
ACC2="SRR341577"
ACC3="SRR341576"
ACC4="SRR341575"

TOOL="fasterq-dump"

#create the output directory:
OUTDIR="${SCRATCH_DIR}/APPEND_TEST"
if [ -d "$OUTDIR" ]; then
    rm -rf "$OUTDIR"
fi

mkdir -p "$OUTDIR"

#create the regular output ( no append-mode ) for later comparison:
for ACC in $ACC1 $ACC2 $ACC3 $ACC4
do
    $TOOL $ACC -O $OUTDIR -t $TEMP_DIR -p
    #append the output of these for later comparison
    cat "${OUTDIR}/${ACC}_1.fastq" >> "${OUTDIR}/A_1.fastq"
    cat "${OUTDIR}/${ACC}_2.fastq" >> "${OUTDIR}/A_2.fastq"
    rm "${OUTDIR}/${ACC}_1.fastq" "${OUTDIR}/${ACC}_2.fastq"
done

#run the tool in append-mode
for ACC in $ACC1 $ACC2 $ACC3 $ACC4
do
    $TOOL $ACC -o "${OUTDIR}/B.fastq" -t $TEMP_DIR -p --append
done

OPT1="--brief"
OPT2="--report-identical-files"
diff "$OPT1" "$OPT2" "${OUTDIR}/A_1.fastq" "${OUTDIR}/B_1.fastq"
diff "$OPT1" "$OPT2" "${OUTDIR}/A_2.fastq" "${OUTDIR}/B_2.fastq"

#get rid of all the data at the end
if [ -d "$OUTDIR" ]; then
    rm -rf "$OUTDIR"
fi

echo "done"
