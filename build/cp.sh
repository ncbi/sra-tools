#!/bin/bash

SRC="$1"
DST="$2"
QUERY="$3"

# go to where this directory may be found
cd "$SRC" || exit $?

# find all of the files in question
for f in $(find . $QUERY)
do

    # test for being newer
    if [ $f -nt "$DST/$f" ]
    then

        # test if destination exists
        if [ ! -f "$DST/$f" ]
        then

            # make sure the directory exists
            d=$(dirname "$DST/$f")
            mkdir -p "$d"
        fi

        # copy
        echo "cp $SRC/$f $DST/$f"
        cp -pPR $f $DST/$f
    fi

done
