echo "----- read NGS in style: ALIGNMENTS -----"

TOOL="vdb-sql"
ACC="SRR341578"

TMPFILE=`mktemp -u`

#create a virtual table named SRC on our accession
echo "create virtual table NGS using ngs( $ACC, style = ALIGNMENTS );" >> $TMPFILE

#set the separator to new-line
echo ".mode line" >> $TMPFILE

#compose the output from 4 values...
echo "select * from NGS limit 2;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
