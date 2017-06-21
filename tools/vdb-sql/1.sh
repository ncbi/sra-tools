TOOL="$HOME/devel/ncbi2/OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR341578"
COLS="READ"
VTBL="SRA"
CREATE="create virtual table $VTBL using vdb( acc=$ACC, columns=$COLS );"
SELECT="select * from $VTBL limit 5;"

set -f
CMD="$TOOL :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
