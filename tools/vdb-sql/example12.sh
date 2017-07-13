echo "----- produce FASTQ ( with split-spot and a view ! ) -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"

if [ ! -f $TOOL ]; then
TOOL="../../../OUTDIR/sra-tools/mac/clang/x86_64/dbg/bin/vdb-sql"
fi

if [ ! -f $TOOL ]; then
    echo "tool not found"
    exit 1
fi

ACC="SRR341577"

TMPFILE=`mktemp -u`

#create a virtual table named FASTQ on our accession
echo "create virtual table FASTQ using vdb( $ACC, " >> $TMPFILE
echo "   columns = NAME;READ;(INSDC:quality:text:phred_33)QUALITY;READ_START;READ_LEN );" >> $TMPFILE

#set the separator to new-line
echo ".separator \"\n\"" >> $TMPFILE

#create a view
echo "create temp view if not exists SEGMENTS ( START0, START1, LEN0, LEN1 ) as" >> $TMPFILE
echo "select" >> $TMPFILE
echo "json_extract( READ_START, '$.a[0]' ) + 1," >> $TMPFILE
echo "json_extract( READ_START, '$.a[1]' ) + 1," >> $TMPFILE
echo "json_extract( READ_LEN, '$.a[0]' )," >> $TMPFILE
echo "json_extract( READ_LEN, '$.a[1]' )" >> $TMPFILE
echo "from FASTQ;" >> $TMPFILE

#compose the output from 4 values...
echo "select" >> $TMPFILE

echo " printf( '@%s.%s %s length=%s', '$ACC', FASTQ.NAME, FASTQ.NAME, SEGMENTS.LEN0 ), " >> $TMPFILE
echo " substr( FASTQ.READ, SEGMENTS.START0, SEGMENTS.LEN0 ), " >> $TMPFILE
echo " printf( '+%s.%s %s length=%d', '$ACC', FASTQ.NAME, FASTQ.NAME, SEGMENTS.LEN0 ), " >> $TMPFILE
echo " substr( FASTQ.QUALITY, SEGMENTS.START0, SEGMENTS.LEN0 ), " >> $TMPFILE

echo " printf( '@%s.%s %s length=%s', '$ACC', FASTQ.NAME, FASTQ.NAME, SEGMENTS.LEN1 ), " >> $TMPFILE
echo " substr( FASTQ.READ, SEGMENTS.START1, SEGMENTS.LEN1 ), " >> $TMPFILE
echo " printf( '+%s.%s %s length=%d', '$ACC', FASTQ.NAME, FASTQ.NAME, SEGMENTS.LEN1 ), " >> $TMPFILE
echo " substr( FASTQ.QUALITY, SEGMENTS.START1, SEGMENTS.LEN1 ) " >> $TMPFILE

echo "from FASTQ, SEGMENTS LIMIT 12;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
