echo "----- produce FASTQ -----"

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

ACC="SRR341577"

TMPFILE=`mktemp -u`

#create a virtual table named FASTQ on our accession
echo "create virtual table FASTQ using vdb( $ACC, " >> $TMPFILE
echo "   columns = NAME;READ;(INSDC:quality:text:phred_33)QUALITY );" >> $TMPFILE

#set the separator to new-line
echo ".separator \"\n\"" >> $TMPFILE

#compose the output from 4 values...
echo "select" >> $TMPFILE
echo " printf( '@%s.%s %s length=%d', '$ACC', NAME, NAME, length( READ ) )," >> $TMPFILE
echo " READ," >> $TMPFILE
echo " printf( '+%s.%s %s length=%d', '$ACC', NAME, NAME, length( QUALITY ) )," >> $TMPFILE
echo " QUALITY" >> $TMPFILE
echo "from FASTQ LIMIT 10;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
