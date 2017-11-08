TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/fastconv"
ACC="SRR341578"

rm -f $ACC.db $ACC.fastq
$TOOL $ACC
