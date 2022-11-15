BINDIR="$1"
TESTBINDIR="$2"

COMMON="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg"
if [ "$BINDIR" == "" ]; then
    BINDIR="$COMMON/bin"
fi

if [ "$TESTBINDIR" == "" ]; then
    TESTBINDIR="$COMMON/test-bin"
fi

if [ ! -d $BINDIR ]; then
    echo "$BINDIR not found"
    exit 3
fi

if [ ! -d $TESTBINDIR ]; then
    echo "$TESTBINDIR not found"
    exit 3
fi

export BINDIR TESTBINDIR

cat ./1.txt | ./build.py
