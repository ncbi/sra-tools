ACC1="SRR341578"
ACC2="SRR5450996"

MIN=4000
MAX=4100

TOOL="$HOME/devel/ncbi2/OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/ref-idx.2.8.2"

#$TOOL $ACC2 -f 1
#$TOOL $ACC2 --min 1000000

#$TOOL $ACC2 -f 3 --slice NC_000067.6:183295000.20

$TOOL './' -f 4
