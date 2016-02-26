BINDIR=$1
mode=$2

have_access()
{
	$BINDIR/vdb-config -on /repository/site > /dev/null
}

var_expand()
{
	if have_access; then
		echo "Starting var-expand test...";
		rm -f var-expand.out;
		echo "a CM000664.1:234668879:14:AT" | $BINDIR/var-expand --algorithm=ra >> var-expand.out;
		diff expected/var-expand.out var-expand.out;
		rm var-expand.out;
		echo "var-expand test is done";
	else
		echo "no access to repository, skipping var-expand test...";
	fi
}

ref_variation()
{
	if have_access; then 
		echo "Starting ref-variation test..."; 
		./ref-variation.sh $BINDIR; 
		echo "ref-variation test is done";	
	else 
		echo "no access to repository, skipping ref-variation test..."; 
	fi
}

if [ "$mode" = "ref-variation" ]; then
	ref_variation
elif [ "$mode" = "var-expand" ]; then
	var_expand
fi
