#ACC="SRR2989969"
ACC="ERR1305324"

echo "testing ${ACC}"

SCRATCH="/panfs/traces01/sra_review/scratch/raetzw"
OUTDIR="${SCRATCH}/${ACC}"
mkdir -p "${OUTDIR}"
OUTDIR1="${OUTDIR}/fastq-dump"
OUTDIR2="${OUTDIR}/fasterq-dump"
TEMPDIR="${OUTDIR}/tmp"

run_tool()
{
    TOOL="$1"
    OUT="$2"
    TMP="$3"
    echo "."
    echo "----------------------------------------------------------------"
    CMD="${TOOL} --split-3 --skip-technical --outdir ${OUT} ${TMP} ${ACC}"
    echo $CMD
    $CMD
}

run_tool "fastq-dump" "${OUTDIR1}"
run_tool "fasterq-dump" "${OUTDIR2}" "-t $TEMPDIR --force -p"

perform_compare()
{
    F1="$1"
    F2="$2"
    if [ -f "${F1}" ]; then
        if [ -f "${F2}" ]; then    
            echo "."
            echo "----------------------------------------------------------------"
            echo "comparing ${F1} vs ${F2}"
            diff -qs "${F1}" "${F2}"
        else
            echo "${F2} missing"
        fi
    else
        echo "${F1} missing"
    fi
}

perform_compare "${OUTDIR1}/${ACC}.fastq" "${OUTDIR2}/${ACC}.fastq"
perform_compare "${OUTDIR1}/${ACC}_1.fastq" "${OUTDIR2}/${ACC}_1.fastq"
perform_compare "${OUTDIR1}/${ACC}_2.fastq" "${OUTDIR2}/${ACC}_2.fastq"
