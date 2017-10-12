echo "----- which alignments are the same between 2 accessions -----"

# this example does the same as example6/example7
# the only difference is that it creates a temporary script and pipes it into the tool

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

ACC1="SRR341578"
ACC2="SRR341580"

TMPFILE=`mktemp -u`
echo "create virtual table VDB1 using vdb( $ACC1, table=PRIM );" >> $TMPFILE
echo "create virtual table VDB2 using vdb( $ACC2, table=PRIM );" >> $TMPFILE
echo "select VDB1.ALIGN_ID, VDB2.ALIGN_ID, VDB1.READ from VDB1, VDB2 WHERE VDB1.READ = VDB2.READ LIMIT 2" >> $TMPFILE
$TOOL < $TMPFILE
rm $TMPFILE
