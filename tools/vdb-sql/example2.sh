echo "----- list of References -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR576646"
SELECT="select SEQ_ID, NAME, count( SEQ_ID ), sum( READ_LEN ) from VDB group by SEQ_ID;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: -acc $ACC -tbl REF \"$SELECT\""
echo $CMD
eval $CMD
