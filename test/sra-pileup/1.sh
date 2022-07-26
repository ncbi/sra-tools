BINDIR="/home/$USER/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
SAMDUMP="${BINDIR}/sam-dump"
SAMVIEW="${BINDIR}/samview"
ACC="SRR341578"
SRC="/mnt/gera/SRA/ACC/cSRA/$ACC"

#generate a sam-file with options
generate_sam() {
    GS_NR="$1"
    GS_OPTS="$2"
    GS_OUT="${ACC}_${GS_NR}.sam"
    if [ -f $GS_OUT ]; then
        echo "${GS_OUT} already exists!"
    else
        GS_CMD="${SAMDUMP} ${SRC} ${GS_OPTS} > ${GS_OUT}"
        echo "${GS_CMD}"
        eval "time ${GC_CMD}"
    fi
}

#generate sam-files  with/without unaligned and with/without qualities
generate_sam_files() {
    generate_sam "1" "-u"
    generate_sam "2" "-u -o"
    generate_sam "3" ""
    generate_sam "4" "-o"
}

verify_sam_files() {
    for NR in 1 2 3 4; do
        VS_TO_TEST="${ACC}_${NR}.sam"
        echo "TESTING: ${VS_TO_TEST}"
        VS_TEST_RESULT="${ACC}_${NR}.result"
        if [ -f ${VS_TEST_RESULT} ]; then
            echo "${VS_TEST_RESULT} already exists!"
        else
            cat "${ACC}_${NR}.sam" | $SAMVIEW > /dev/null
            echo "result ${TO_TEST} = $?" > ${VS_TEST_RESULT}
            cat ${VS_TEST_RESULT}
        fi
    done
}

generate_sam_files
verify_sam_files
