TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR341578"

#COLS="READ;READ_LEN"
#CREATE="create virtual table VDB using vdb( $ACC, columns=$COLS );"
#CREATE="create virtual table VDB using vdb( $ACC, exclude=NAME );"
CREATE="create virtual table VDB using vdb( $ACC );"


#SELECT="select READ, READ_START, json_extract( json_object( 'a', READ_START ), '$.a[1]' ) from VDB limit 3;"

#SELECT="select READ, READ_START, json_extract( json_object( 'a', json( READ_START ) ), '$.a[1]' ) from VDB limit 3;"

#SELECT="select json_each.value, count( json_each.value ) from VDB, json_each( VDB.READ_FILTER ) group by json_each.value;"

SELECT="select * from VDB limit 1;"

#to prevent the shell from expanding '*' into filenames!
set -f

#CMD="$TOOL -line :memory: \"$CREATE $SELECT\""
#CMD="$TOOL -line -acc $ACC :memory: \"$SELECT\""
#echo $CMD
#eval $CMD


TMPFILE=`mktemp -u`

#create a virtual table named FASTQ on our accession
echo "create virtual table NGS using ngs( $ACC, style = PILEUP, ref = NC_011748.1 );" >> $TMPFILE
#echo ".mode line" >> $TMPFILE
#get something out of it...
echo "select * FROM NGS WHERE DEPTH > 130;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
