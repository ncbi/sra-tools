
BINDIR="$1"
TESTBINDIR="$2"
SAMLINE="$TESTBINDIR/samline"
SAMPART="$TESTBINDIR/sampart"
BAMLOAD="$BINDIR/bam-load"
SRASORT="$BINDIR/sra-sort"
VDBDUMP="$BINDIR/vdb-dump"

if [ ! -f $SAMLINE ]; then
    echo "$SAMLINE not found"
    exit 3
fi

if [ ! -f $SAMPART ]; then
    echo "$SAMPART not found"
    exit 3
fi

if [ ! -f $BAMLOAD ]; then
    echo "$BAMLOAD not found"
    exit 3
fi

if [ ! -f $SRASORT ]; then
    echo "$SRASORT not found"
    exit 3
fi

if [ ! -f $VDBDUMP ]; then
    echo "$VDBDUMP not found"
    exit 3
fi

export BINDIR TESTBINDIR
#./simpletest.py
./ca_test.py
exit $?
