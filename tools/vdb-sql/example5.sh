echo "----- count how many different READ_FILTER values we have -----"

TOOL="vdb-sql"
ACC="SRR341578"
VALUE="json_each.value"
SELECT="select $VALUE, count( $VALUE ) from VDB, json_each( VDB.READ_FILTER ) group by $VALUE;"

#to prevent the shell from expanding '*' into filenames!
set -f

CMD="$TOOL :memory: -acc $ACC \"$SELECT\""
echo $CMD
eval $CMD
