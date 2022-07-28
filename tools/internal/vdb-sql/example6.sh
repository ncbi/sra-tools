echo "----- count how many alignments we have in a given slice -----"

TOOL="vdb-sql"
ACC="SRR341578"
ID="NC_011748.1"
START="123456"
END="133456"
SELECT="select count() from VDB where REF_SEQ_ID='$ID' and REF_POS BETWEEN $START AND $END;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: -acc $ACC -tbl PRIM \"$SELECT\""
echo $CMD
eval $CMD
