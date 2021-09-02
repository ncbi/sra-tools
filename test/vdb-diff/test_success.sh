BINDIR=$1
ACCESSION=$2

rm -rf A1 A2
$BINDIR/vdb-copy $ACCESSION A1 -R 1-10
$BINDIR/vdb-copy $ACCESSION A2 -R 1-10
$BINDIR/vdb-diff A1 A2
RESULT="$?"
rm -rf A1 A2

if [ $RESULT -eq 0 ]; then
    echo "test (compare identical objects) passed for $BINDIR/vdb-diff"
else
    echo "test (compare identical objects) failed for $BINDIR/vdb-diff"
fi

exit $RESULT
