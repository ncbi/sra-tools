#!/usr/bin/env bash

# common helper script to build the sam-factory tool
# to be called by other scripts...
#

SAMFACTORY="./sam-factory"
SAMFACTORYSRC="./$SAMFACTORY.cpp"

#check if we have the source-code for the sam-factory tool
if [[ ! -f "$SAMFACTORYSRC" ]]; then
    echo "$SAMFACTORYSRC not found"
    exit 3
fi

#if the sam-factory tool alread exists, remove it
if [[ -f "$SAMFACTORY" ]]; then
    rm -f "$SAMFACTORY"
fi

#build the sam-factory tool
g++ $SAMFACTORYSRC -o $SAMFACTORY
if [[ ! -x "$SAMFACTORY" ]]; then
    echo "$SAMFACTOR could not be build"
    exit 3
fi

echo "sam-factory-tool produced!"
