echo "----- count how many of each READ_FILTER values we have -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR341578"

CREATE="create virtual table SRA using vdb( $ACC, T = PRIM );"
ID="NC_011748.1"
START="123456"
END="133456"
SELECT="select count() from SRA where REF_SEQ_ID='$ID' and REF_POS >= $START and REF_POS <= $END;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
