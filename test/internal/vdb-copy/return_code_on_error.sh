#!/bin/sh

VDB_COPY="vdb-copy"
if [ ! -x $VDB_COPY ]; then
    TOOL_PATH="$1"
    VDB_COPY="${TOOL_PATH}/$VDB_COPY"
fi

if [ ! -x $VDB_COPY ]; then
    echo "cannot find executable for vdb-copy"
    exit 3
fi

if $VDB_COPY NOT_EXISTING_DIR LOCAL_COPY ; then
    echo "ERROR: this should not succeed!"
    exit 3
else
    echo "SUCCESS: vdb-copy returned $? if ask to copy a none-existing object."
    exit 0
fi
