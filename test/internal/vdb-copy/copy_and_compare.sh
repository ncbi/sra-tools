#!/bin/sh
set -e

TOOL_PATH="$1"

VDB_COPY="vdb-copy"
if [ ! -x $VDB_COPY ]; then
    VDB_COPY="${TOOL_PATH}/$VDB_COPY"
fi

if [ ! -x $VDB_COPY ]; then
    echo "cannot find executable for vdb-copy"
    exit 3
fi

ACC="$2"
ACC_COPY="the_copy"

if [ -d $ACC_COPY ]; then
    rm -rf $ACC_COPY
fi

BIN_PATH="$3"

VDB_DIFF="vdb-diff"
if [ ! -x $VDB_DIFF ]; then          # try vdb-diff from DIRTOTEST
    VDB_DIFF="${TOOL_PATH}/$VDB_DIFF"
fi
if [ ! -x $VDB_DIFF ]; then          # try vdb-diff from BINDIR
    VDB_DIFF="${BIN_PATH}/vdb-diff"
fi
if [ ! -x $VDB_DIFF ]; then
    echo "cannot find executable for vdb-diff"
    exit 3
fi

export NCBI_SETTINGS=/

$VDB_COPY $ACC $ACC_COPY -p

$VDB_DIFF $ACC $ACC_COPY -pc

if [ -d $ACC_COPY ]; then
    rm -rf $ACC_COPY
fi
