
TESTBINS="$1"

CMD="$TESTBINS/samline -h"
echo $CMD
eval $CMD
exit $?
