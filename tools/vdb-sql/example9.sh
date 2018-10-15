echo "----- which alignments are the same between 2 accessions -----"

# this example does the same as example6/example7
# the only difference is that it creates a temporary script and pipes it into the tool

TOOL="vdb-sql"
ACC1="SRR341578"
ACC2="SRR341580"

TMPFILE=`mktemp -u`
echo "create virtual table VDB1 using vdb( $ACC1, table=PRIM );" >> $TMPFILE
echo "create virtual table VDB2 using vdb( $ACC2, table=PRIM );" >> $TMPFILE
echo "select VDB1.ALIGN_ID, VDB2.ALIGN_ID, VDB1.READ from VDB1, VDB2 WHERE VDB1.READ = VDB2.READ LIMIT 2" >> $TMPFILE
$TOOL < $TMPFILE
rm $TMPFILE
