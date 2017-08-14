echo "----- read NGS in style: REFS -----"

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

echo "create virtual table NGS using ngs( $ACC, style = REFS );" >> $TMPFILE
echo ".mode line" >> $TMPFILE
echo "select * from NGS limit 2;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
