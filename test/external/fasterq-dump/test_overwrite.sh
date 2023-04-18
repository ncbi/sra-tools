#! /bin/bash

TOOL="fasterq-dump"
if [ ! -x $ACC_C ]; then
    BINDIR="${HOME}/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
    TOOL="${BINDIR}/$TOOL"
fi

if [ ! -x $ACC_C ]; then
    echo "ERROR cannot find fasterq-dump executable"
    exit 3
fi

ACC="ERR1681325"
OUTDIR="OUTDIR"

echo -e "\nTEST #1 ... overwrite existing files without -f"
rm -rf $OUTDIR
if $TOOL $ACC -O $OUTDIR ; then
    if $TOOL $ACC -O $OUTDIR ; then
        echo "SUCCESS: overwriting existing output-files"
    else
        echo "ERROR cannot run '$TOOL $ACC' to overwrite existing output-files"
        rm -rf $OUTDIR
        exit 3
    fi
else
    echo "ERROR cannot run '$TOOL $ACC'"
    rm -rf $OUTDIR
    exit 3
fi

echo -e "\nTEST #2 ... cannot overwrite existing files if permission is missing"
chmod -wx $OUTDIR
if $TOOL $ACC -O $OUTDIR ; then
    echo "ERROR this should not succeed"
    chmod +wx $OUTDIR
    rm -rf $OUTDIR
    exit 3
else
    echo "SUCCESS: overwriting failed as expected"
    chmod +wx $OUTDIR
    rm -rf $OUTDIR
fi

echo -e "\nTEST #3 ... cannot use temp. directory if permission is missing"
TMPDIR="TMPDIR"
mkdir $TMPDIR
chmod -wx $TMPDIR
if $TOOL $ACC -O $OUTDIR -t $TMPDIR ; then
    echo "ERROR this should not succeed"
    chmod +wx $TMPDIR
    rm -rf $OUTDIR $TMPDIR
    exit 3
else
    echo "SUCCESS: using tempdir failed as expected"
    chmod +wx $TMPDIR
    rm -rf $TMPDIR
fi

#cleaning up
if [ -d $OUTDIR ]; then
    chmod +wx $OUTDIR
    rm -rf $OUTDIR
fi
echo -e "\nDONE."
