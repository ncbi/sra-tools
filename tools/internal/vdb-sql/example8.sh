echo "----- count how many alignments we have in a given slice -----"

# this example does the same as example6/example7
# the only difference is that it creates a temporary script and pipes it into the tool

TOOL="vdb-sql"
ACC="SRR341578"
ID="NC_011748.1"
START="123456"
END="133456"

TMPFILE=`mktemp -u`
echo "create virtual table VDB using vdb( $ACC, table=PRIM );" > $TMPFILE
echo "select count() from VDB where REF_SEQ_ID='$ID' and REF_POS >= $START and REF_POS <= $END;" >> $TMPFILE
$TOOL < $TMPFILE
rm $TMPFILE
