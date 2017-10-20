echo "----- count how many alignments we have in a given slice -----"

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
ID="NC_011748.1"
START="123456"
END="133456"
SELECT="select count() from VDB where REF_SEQ_ID='$ID' and REF_POS BETWEEN $START AND $END;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: -acc $ACC -tbl PRIM \"$SELECT\""
echo $CMD
eval $CMD
