echo "----- filtering by some bases -----"

TOOL="vdb-sql"
ACC="SRR341578"
SELECT="select READ from VDB where READ like '%ACGGACGGTT%' limit 3;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: -acc $ACC \"$SELECT\""
echo $CMD
eval $CMD
