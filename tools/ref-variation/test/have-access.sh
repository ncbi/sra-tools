BINDIR=$1
test_name=$2
exit_code=0
have_access()
{   # expect to have vdb-config in the PATH
	vdb-config -on /repository/site > /dev/null
}

if have_access; then
	echo "Starting $test_name test...";
	if [ "$test_name" = "ref-variation" ]; then
		./ref-variation.sh $BINDIR;
		exit_code=$?
	elif [ "$test_name" = "var-expand" ]; then
		rm -f var-expand.out
		echo "a CM000664.1:234668879:14:AT" | $BINDIR/var-expand --no-user-settings --algorithm=ra >> var-expand.out
		diff expected/var-expand.out var-expand.out
		exit_code=$?
		rm var-expand.out
	fi
	echo "$test_name test is done";
else
	echo "no access to repository, skipping $test_name test...";
fi
exit $exit_code