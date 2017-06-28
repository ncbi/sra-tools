echo "----- count how many alignments start in a given slice -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR341578"

CREATE="create virtual table SRA using vdb( $ACC );"
VALUE="json_each.value"
SELECT="select $VALUE, count( $VALUE ) from SRA, json_each( SRA.READ_FILTER ) group by $VALUE;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
