#!/bin/bash +x

# hard-coded for the moment
DEPLOYQT=~/Qt/5.10.0/clang_64/bin/macdeployqt

if [ $# -lt 4 ]
then
    echo "Usage: $0 APPNAME VDB_LIBDIR VDB_VERSION VDB_LIB ..."
    exit 1
fi

APPNAME="$1"
VDB_LIBDIR="$2"
VDB_VERSION="$3"
shift 3

APPDIR="$APPNAME"

# produce the bundle, but without VDB libraries
$DEPLOYQT "$APPDIR"

# ensure the library directory is there
mkdir -p "$APPDIR/Contents/lib"

# copy the required libraries
for LIB in $*
do
    LIBPATH="$VDB_LIBDIR/lib$LIB.$VDB_VERSION.dylib"
    if [ ! -x "$LIBPATH" ]
    then
	echo "required library '$LIBPATH' not found"
	exit 2
    else
	cp -f "$LIBPATH" "$APPDIR/Contents/lib"
    fi
done

exit 0
