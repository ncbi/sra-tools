BINDIR=$1

function Case(){
    TESTCASE=$1
    shift
    PARAMS=$*
	cmd="$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info $PARAMS 1>$TESTCASE.out 2>$TESTCASE.err"
    echo "$cmd"
    eval $cmd
    diff expected/$TESTCASE.out $TESTCASE.out >>diff.out
}

if [ "$(uname)" = "Darwin" ]
then
	echo "ref-variation test is disabled for Mac"
else
	rm -f *.out
	Case 1 -r NC_000013.10 -p 100635036 --query ACC -l 0 SRR793062 SRR795251
	Case 2 -r NC_000002.11 -p 73613071 --query "C" -l 1
	Case 3 -t 16 -r NC_000007.13 -p 117292900 --query "-" -l 4
	Case 4 -c -t 1 -r NC_000002.11 -p 73613067 --query "-" -l 3 SRR867061 SRR867131
	Case 5 -c --count-strand counteraligned -t 1 -r NC_000002.11 -p 73613067 --query "-" -l 3 SRR867061 SRR867131
	Case 6 -c -r CM000671.1 -p 136131022 --query "T" -l 1 SRR1601768
	Case 7 -c -r NC_000001.11 -p 136131022 --query "T" -l 1 SRR1601768
	Case 8 -t 16 -c -r CM000671.1 -p 136131021 --query "T" -l 1 SRR1596639
	Case 9 -r NC_000002.11 -p 73613030 --query "AT[1-3]" -l 3
	Case 10 -c -r CM000664.1 -p 234668879  -l 14 --query "ATATATATATATAT" SRR1597895
	Case 11 -c -r CM000664.1 -p 234668879  -l 14 --query "AT[1-8]" SRR1597895
	Case 12 -r NC_000001.10 -p 1064999 -l 1 --query A
	Case 13 -r NC_000020.10 -p 137534 -l 2 --query -
	Case 14 -c -r CM000663.1 -p 123452 -l 7 --query "GGGAGAAAT"
	Case 15 -r CM000684.1 -p 36662045 --query - -l 6 -c SRR1597729
	Case 16 -r NC_000001.10 -p 570000 -l 1 --query A 2>&1 > 16.out
	Case 17 -L err -c -t 1 -r NC_000002.11 -p 73613067 --query "-" -l 3 -i ref-variation.in
    if [ -s diff.out ]; 
    then
        exit 1
    fi
fi
