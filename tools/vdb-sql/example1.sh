echo "----- filtering by some bases -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR341578"

CREATE="create virtual table SRA using vdb( $ACC );"
SELECT="select READ from SRA where READ like '%ACGGACGGTT%' limit 3;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL -line :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
