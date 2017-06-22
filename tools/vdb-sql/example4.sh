echo "----- show the distribution of REF_LEN of all alignments -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR341578"

CREATE="create virtual table SRA using vdb( $ACC, T=PRIM );"
SELECT="select REF_LEN, count( REF_LEN ) from SRA group by REF_LEN;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
