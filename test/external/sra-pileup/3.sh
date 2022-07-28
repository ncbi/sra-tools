TOOLDIR="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
ACC="SRR3697723"

run() {
./test-sam-dump.py --exe "${TOOLDIR}" --acc "${ACC}"
echo "result: $?"
}

ttt() {
TOOL="${TOOLDIR}/vdb-dump"
PRIM_COLS="ALIGN_ID,SEQ_SPOT_ID,SAM_FLAGS,REF_NAME,REF_POS,MAPQ,CIGAR_SHORT,MATE_REF_NAME,MATE_REF_POS,TEMPLATE_LEN,READ,SAM_QUALITY"
CMD="${TOOL} ${ACC} -T PRIM -f csv -C ${PRIM_COLS}"
eval $CMD
}

run
#ttt
