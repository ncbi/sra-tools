echo "----- pick READ and QUALITY from different tables -----"

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

SRC="SRR341578"
DST="$SRC.db"

SRC_COLS_A="ALIGN_ID;SEQ_SPOT_ID;READ"
DST_COLS_A="ALIGN_ID INTEGER, SEQ_SPOT_ID INTEGER, READ"

rm -f $DST

TMPFILE=`mktemp -u`

#create a virtual table to read from
echo "create virtual table SRC_ALIG using vdb( $SRC, table = PRIMARY_ALIGNMENT, columns = $SRC_COLS_A );" >> $TMPFILE

#create a real table
echo "create table ALIG( $DST_COLS_A );" >> $TMPFILE

#copy the data from the virtual table into the real one
echo "insert into ALIG select * from SRC_ALIG;" >> $TMPFILE

#create an index on SEQ_SPOT_ID
echo "create index I_SEQ_SPOT_ID on ALIG( SEQ_SPOT_ID );" >> $TMPFILE

echo "performing copy"
$TOOL $DST < $TMPFILE

rm $TMPFILE
