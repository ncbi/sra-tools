echo "----- read NGS in style: REFS -----"

TOOL="vdb-sql"
ACC="SRR341578"

TMPFILE=`mktemp -u`

echo "create virtual table NGS using ngs( $ACC, style = REFS );" >> $TMPFILE
echo ".mode line" >> $TMPFILE
echo "select * from NGS limit 2;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
