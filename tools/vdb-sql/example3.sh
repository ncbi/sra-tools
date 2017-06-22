echo "----- what spot-groups do we have -----"

TOOL="../../../OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/vdb-sql"
ACC="SRR834507"

CREATE="create virtual table SRA using vdb( $ACC );"
SELECT="select SPOT_GROUP, count( SPOT_GROUP ) from SRA group by SPOT_GROUP;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: \"$CREATE $SELECT\""
echo $CMD
eval $CMD
