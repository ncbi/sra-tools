echo "----- read NGS in style: PILEUP -----"

TOOL="vdb-sql"
ACC="SRR341578"

TMPFILE=`mktemp -u`

#create a virtual table named SRC on our accession
echo "create virtual table NGS using ngs( $ACC, style = PILEUP, ref = NC_011748.1 );" >> $TMPFILE

#compose the output from 4 values...
echo "select * from NGS limit 1;" >> $TMPFILE

echo "select POS, DEPTH FROM NGS WHERE DEPTH > 130 GROUP BY POS;" >> $TMPFILE
$TOOL < $TMPFILE

rm $TMPFILE
