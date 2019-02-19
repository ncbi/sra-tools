echo "----- read NGS in style: READGROUPS -----"

TOOL="vdb-sql"
ACC="SRR341578"

TMPFILE=`mktemp -u`

echo "create virtual table NGS using ngs( $ACC, style = READGROUPS );" >> $TMPFILE
echo ".mode line" >> $TMPFILE
echo "select * from NGS;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
