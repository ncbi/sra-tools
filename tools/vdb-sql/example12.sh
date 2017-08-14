echo "----- produce FASTQ ( with split-spot and a view ! ) -----"

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

#create a virtual table named SRC on our accession
echo "create virtual table SRC using vdb( $ACC, " >> $TMPFILE
echo "   columns = SPOT_ID;NAME;READ;(INSDC:quality:text:phred_33)QUALITY;READ_START;READ_LEN );" >> $TMPFILE

#set the separator to new-line
echo ".separator \"\n\"" >> $TMPFILE

#create a view for start/len values taken out of READ_START and READ_LEN
echo "create temp view if not exists SEGMENTS ( START0, START1, LEN0, LEN1 ) as" >> $TMPFILE
echo "select" >> $TMPFILE
echo "json_extract( READ_START, '$.a[0]' ) + 1," >> $TMPFILE
echo "json_extract( READ_START, '$.a[1]' ) + 1," >> $TMPFILE
echo "json_extract( READ_LEN, '$.a[0]' )," >> $TMPFILE
echo "json_extract( READ_LEN, '$.a[1]' )" >> $TMPFILE
echo "from SRC;" >> $TMPFILE

#compose the output from 4 values...
echo "select" >> $TMPFILE

echo " printf( '@%s.%s %s length=%s', '$ACC', SRC.SPOT_ID, SRC.NAME, SEGMENTS.LEN0 ), " >> $TMPFILE
echo " substr( SRC.READ, SEGMENTS.START0, SEGMENTS.LEN0 ), " >> $TMPFILE
echo " printf( '+%s.%s %s length=%d', '$ACC', SRC.SPOT_ID, SRC.NAME, SEGMENTS.LEN0 ), " >> $TMPFILE
echo " substr( SRC.QUALITY, SEGMENTS.START0, SEGMENTS.LEN0 ), " >> $TMPFILE

echo " printf( '@%s.%s %s length=%s', '$ACC', SRC.SPOT_ID, SRC.NAME, SEGMENTS.LEN1 ), " >> $TMPFILE
echo " substr( SRC.READ, SEGMENTS.START1, SEGMENTS.LEN1 ), " >> $TMPFILE
echo " printf( '+%s.%s %s length=%d', '$ACC', SRC.SPOT_ID, SRC.NAME, SEGMENTS.LEN1 ), " >> $TMPFILE
echo " substr( SRC.QUALITY, SEGMENTS.START1, SEGMENTS.LEN1 ) " >> $TMPFILE

echo "from SRC, SEGMENTS LIMIT 12;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
