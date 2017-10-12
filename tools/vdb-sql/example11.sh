echo "----- produce FASTQ ( with split-spot !) -----"

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
echo "create virtual table SRC using vdb( $ACC, " >> $TMPFILE
echo "   columns = SPOT_ID;NAME;READ;(INSDC:quality:text:phred_33)QUALITY;READ_START;READ_LEN );" >> $TMPFILE

#set the separator to new-line
echo ".separator \"\n\"" >> $TMPFILE

#compose the output from 4 values...
echo "select" >> $TMPFILE

echo " printf( '@%s.%s %s length=%s', '$ACC', SPOT_ID, NAME, json_extract( READ_LEN, '$.a[0]' ) ), " >> $TMPFILE
echo " substr( READ, json_extract( READ_START, '$.a[0]' ) + 1, json_extract( READ_LEN, '$.a[0]' ) ), " >> $TMPFILE
echo " printf( '+%s.%s %s length=%d', '$ACC', SPOT_ID, NAME, json_extract( READ_LEN, '$.a[0]' ) ), " >> $TMPFILE
echo " substr( QUALITY, json_extract( READ_START, '$.a[0]' ) + 1, json_extract( READ_LEN, '$.a[0]' ) ), " >> $TMPFILE
echo " printf( '@%s.%s %s length=%s', '$ACC', SPOT_ID, NAME, json_extract( READ_LEN, '$.a[1]' ) ), " >> $TMPFILE
echo " substr( READ, json_extract( READ_START, '$.a[1]' ) + 1, json_extract( READ_LEN, '$.a[1]' ) ), " >> $TMPFILE
echo " printf( '+%s.%s %s length=%d', '$ACC', SPOT_ID, NAME, json_extract( READ_LEN, '$.a[1]' ) ), " >> $TMPFILE
echo " substr( QUALITY, json_extract( READ_START, '$.a[1]' ) + 1, json_extract( READ_LEN, '$.a[1]' ) ) " >> $TMPFILE

echo "from SRC LIMIT 12;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
