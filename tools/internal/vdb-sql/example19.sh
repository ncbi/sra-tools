echo "----- copy flat SRA-table to sqlite-db -----"

TOOL="/vdb-sql"
SRC="SRR000001"
DST="SRR000001.db"

SRC_COLS="SPOT_ID;NAME;READ;(INSDC:quality:text:phred_33)QUALITY;READ_START;READ_LEN;READ_FILTER;READ_TYPE"
DST_COLS="SPOT_ID INTEGER PRIMARY KEY, NAME, READ, QUALITY, READ_START, READ_LEN, READ_FILTER, READ_TYPE"

rm -f $DST

TMPFILE=`mktemp -u`

#create a virtual table named SRC on our accession
echo "create virtual table SRC using vdb( $SRC, columns = $SRC_COLS );" >> $TMPFILE

#create a real table named SRR
echo "create table SRR( $DST_COLS );" >> $TMPFILE

#copy the data from the virtual table into the read one
echo "insert into SRR select * from SRC;" >> $TMPFILE

echo "performing copy"
$TOOL $DST < $TMPFILE

rm $TMPFILE
