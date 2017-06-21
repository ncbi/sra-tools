TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR341578"
COLS="READ;READ_LEN"
VTBL="SRA"
#CREATE="create virtual table $VTBL using vdb( $ACC, columns=$COLS );"
CREATE="create virtual table $VTBL using vdb( $ACC );"
SELECT="select READ,SPOT_ID from $VTBL limit 5;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL -line :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
