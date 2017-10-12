echo "----- read NGS in style: PILEUP -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"

if [ ! -f $TOOL ]; then
TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/rel/bin/vdb-sql"
fi

if [ ! -f $TOOL ]; then
TOOL="../../../OUTDIR/sra-tools/mac/clang/x86_64/dbg/bin/vdb-sql"
fi

if [ ! -f $TOOL ]; then
TOOL="../../../OUTDIR/sra-tools/mac/clang/x86_64/rel/bin/vdb-sql"
fi

if [ ! -f $TOOL ]; then
    echo "tool not found"
    exit 1
fi

ACC="SRR341578"

TMPFILE=`mktemp -u`

#create a virtual table named SRC on our accession
echo "create virtual table NGS using ngs( $ACC, style = PILEUP, ref = NC_011748.1 );" >> $TMPFILE

#compose the output from 4 values...
echo "select * from NGS limit 1;" >> $TMPFILE

echo "select POS, DEPTH FROM NGS WHERE DEPTH > 130 GROUP BY POS;" >> $TMPFILE
$TOOL < $TMPFILE

rm $TMPFILE
