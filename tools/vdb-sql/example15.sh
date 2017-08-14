echo "----- read NGS in style: ALIGNMENTS -----"

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
echo "create virtual table NGS using ngs( $ACC, style = ALIGNMENTS );" >> $TMPFILE

#set the separator to new-line
echo ".mode line" >> $TMPFILE

#compose the output from 4 values...
echo "select * from NGS limit 2;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
