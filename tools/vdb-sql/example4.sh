echo "----- show the distribution of REF_LEN of all alignments -----"

TOOL="vdb-sql"
ACC="SRR341578"
SELECT="select REF_LEN, count( REF_LEN ) from VDB group by REF_LEN;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: -acc $ACC -tbl PRIM \"$SELECT\""
echo $CMD
eval $CMD
