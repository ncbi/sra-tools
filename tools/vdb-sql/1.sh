TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR341578"
#COLS="READ;READ_LEN"
#CREATE="create virtual table SRA using vdb( $ACC, columns=$COLS );"
#CREATE="create virtual table SRA using vdb( $ACC, exclude=NAME );"
CREATE="create virtual table SRA using vdb( $ACC );"
#SELECT="select READ, READ_START, json_extract( json_object( 'a', READ_START ), '$.a[1]' ) from SRA limit 3;"

SELECT="select READ, json_extract( json_object( 'a', json( READ_START ) ), '$.a[1]' )  from SRA limit 3;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL -line :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
