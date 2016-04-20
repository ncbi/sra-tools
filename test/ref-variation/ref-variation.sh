BINDIR=$1

if [ "$(uname)" = "Darwin" ]
then
	echo "ref-variation test is disabled for Mac"
else
	rm -f ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -v -r NC_000013.10 -p 100635036 --query ACC -l 0 SRR793062 SRR795251 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -vv -r NC_000002.11 -p 73613071 --query "C" -l 1 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -vv -t 16 -r NC_000007.13 -p 117292900 --query "-" -l 4 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -v -c -t 1 -r NC_000002.11 -p 73613067 --query "-" -l 3 SRR867061 SRR867131 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -v -c --count-strand counteraligned -t 1 -r NC_000002.11 -p 73613067 --query "-" -l 3 SRR867061 SRR867131 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -v -c -r CM000671.1 -p 136131022 --query "T" -l 1 SRR1601768 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -v -c -r NC_000001.11 -p 136131022 --query "T" -l 1 SRR1601768 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -v -t 16 -c -r CM000671.1 -p 136131021 --query "T" -l 1 SRR1596639 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -vv -r NC_000002.11 -p 73613030 --query "AT[1-3]" -l 3 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -v -c -r CM000664.1 -p 234668879  -l 14 --query "ATATATATATATAT" SRR1597895 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -v -c -r CM000664.1 -p 234668879  -l 14 --query "AT[1-8]" SRR1597895 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -vv -r NC_000001.10 -p 1064999 -l 1 --query A 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -vv -r NC_000020.10 -p 137534 -l 2 --query - 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -vv -c -r CM000663.1 -p 123452 -l 7 --query "GGGAGAAAT" 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -vvv -r CM000684.1 -p 36662045 --query - -l 6 -vvv -c SRR1597729 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L info -vv -r NC_000001.10 -p 570000 -l 1 --query A 2>&1 | sed "s/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}\:[0-9]\{2\}\:[0-9]\{2\}[ \t]*ref-variation\.[0-9]\+\.[0-9]\+\(\.[0-9]\+\)\?[ \t]*//" >> ref-variation.out
	$BINDIR/ref-variation --no-user-settings --algorithm=ra -L err -v -c -t 1 -r NC_000002.11 -p 73613067 --query "-" -l 3 -i ref-variation.in >> ref-variation.out 2>&1
	diff expected/ref-variation.out ref-variation.out
	EXIT_CODE=$?
	rm ref-variation.out
	exit $EXIT_CODE
fi
