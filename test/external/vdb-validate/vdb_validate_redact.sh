
#where the binary to be tested is created ( vdb-validate in this case )
SRA_BINDIR="$1"

#where we can find the ncbi-vdb/ncbi-wvdb dynamic libs
#the python-script the produces the test-data needs that
VDB_LIBDIR="$2"

#the include directory where the schemas can be found
#the python-script the produces the test-data needs that
VDB_INCDIR="$3"

#make sure that we have vdb-validate ( the tool to be tested )
TOOL="$SRA_BINDIR/vdb-validate"
if [ ! -x "$TOOL" ]; then
    echo "the tool >$TOOL< cannot be found"
    exit 3
fi

#the test data, to be produces by the python-script
TESTDATA="test-data"

#the python-script that produces the test-data
GENERATOR="generate-test-data-with-unredacted-reads.py"

# the script with the 3 parameters that are neccessary for the script
CMD="python3 $GENERATOR $TESTDATA $VDB_INCDIR $VDB_LIBDIR"
eval "$CMD"

#now we should have the test-data ( as a directory ):
if [ ! -d "$TESTDATA" ]; then
    echo "the testdata >$TESTDATA< was not produced by the python-script"
    exit 3
fi

# let us run the tool on the genereated test-data ( but without --check-redact )
# we should return zero as return-code from vdb-validate
CMD="$TOOL $TESTDATA"
echo "$CMD"
eval "$CMD"
RETCODE="$?"
if [ "$RETCODE" -ne 0 ]; then
    echo ">$CMD< did not result in a zero-return-code, but in >$RETCODE<"
    exit 3
fi

# now let us run the tool with redact-checking on
# we should return none-zero as return-code from vdb-validate
CMD="$TOOL $TESTDATA --check-redact"
echo "$CMD"
eval "$CMD"
RETCODE="$?"
if [ "$RETCODE" -eq 0 ]; then
    echo ">$CMD< did result in a zero-return-code, but it should not"
    exit 3
fi

rm -rf "$TESTDATA"
exit 0
