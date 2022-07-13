echo "----- read NGS in style: FRAGMENTS -----"

TOOL="vdb-sql"
ACC="SRR341577"

TMPFILE=`mktemp -u`

#create a virtual table named SRC on our accession
echo "create virtual table NGS using ngs( $ACC, style = FRAGMENTS );" >> $TMPFILE

#set the separator to new-line
echo ".mode line" >> $TMPFILE

#compose the output from 4 values...
echo "select * from NGS limit 5;" >> $TMPFILE

$TOOL < $TMPFILE

rm $TMPFILE
