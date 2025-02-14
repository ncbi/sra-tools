#! /bin/sh

BIN="$1"

SANDBOX="TEST_SANDBOX"
ACC="SRR053325"

mkdir -p $SANDBOX
cd $SANDBOX

echo "run $BIN $ACC"
$BIN $ACC
STATUS=$?

cd ..
rm -r $SANDBOX

if (( $STATUS == 0 )); then
    echo "success!"
    exit 0
else
    echo "error!"
    exit 3
fi
