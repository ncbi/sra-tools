echo "----- list of References -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR576646"

CREATE="create virtual table SRA using vdb( $ACC, T=REF );"
SELECT="select SEQ_ID, NAME, count( SEQ_ID ), sum( READ_LEN ) from SRA group by SEQ_ID;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
