echo "----- count how many different READ_FILTER values we have -----"

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
VALUE="json_each.value"
SELECT="select $VALUE, count( $VALUE ) from VDB, json_each( VDB.READ_FILTER ) group by $VALUE;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: -acc $ACC \"$SELECT\""
echo $CMD
eval $CMD
