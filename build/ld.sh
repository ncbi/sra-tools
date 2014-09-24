#!/bin/bash
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

# prepare script name
SELF_NAME="$(basename $0)"
BUILD_DIR="$(dirname $0)"
TOP="$(dirname $BUILD_DIR)"
SCRIPT_BASE="${0%.sh}"

# os
OS="$1"
shift

# architecture
ARCH="$1"
shift

# binary loader tool
LD="$1"
shift

# configuration
unset SHLX
unset DYLX
unset LIBX
unset OBJX
unset LOBX

# parameters
TYPE=exe
STATIC=0
DYLD=0
STATICSYSLIBS=0
CHECKSUM=0
KPROC=4
THREADS=0
HAVE_KSPROC=0
NEED_KPROC=1
HAVE_GZIP=0
NEED_GZIP=1
HAVE_BZIP=0
NEED_BZIP=1
HAVE_DL=0
NEED_DL=1
HAVE_M=0
NEED_M=1
HAVE_XML=0
NEED_XML=0
HAVE_KFC=0
HAVE_KAPP=0
HAVE_NCBI_VDB=0
HAVE_NCBI_WVDB=0
unset BUILD
unset LDIRS
unset XDIRS
unset SRCDIR
unset BINDIR
unset VERSFILE
unset VERSDIR
unset TARG
unset EXT
unset OBJS
unset LIBS
unset DEPFILE

# paths for translating local to remote
unset RHOME
unset LHOME
unset RHOST
unset LOUTDIR
unset ROUTDIR
unset PROXY_TOOL

while [ $# -ne 0 ]
do

    case "$1" in
    --build)
        BUILD="$2"
        shift
        ;;

    --ldflags)
        LDFLAGS="$2"
        shift
        ;;

    --static-system-libs)
        STATICSYSLIBS=1
        ;;

    --checksum)
        CHECKSUM=1
        ;;

    --shlx)
        SHLX="$2"
        shift
        ;;

    --dylx)
        SHLX="$2"
        shift
        ;;

    --libx)
        LIBX="$2"
        shift
        ;;

    --objx)
        OBJX="$2"
        shift
        ;;

    --srcdir)
        SRCDIR="$2"
        shift
        ;;

    --bindir)
        BINDIR="$2"
        shift
        ;;
        
    --rhome)
        RHOME="$2"
        shift
        ;;

    --lhome)
        LHOME="$2"
        shift
        ;;

    --loutdir)
        LOUTDIR="$2"
        shift
        ;;

    --routdir)
        ROUTDIR="$2"
        shift
        ;;

    --rhost)
        RHOST="$2"
        shift
        ;;

    --rport)
        RPORT="$2"
        shift
        ;;

    --proxy_tool)
        PROXY_TOOL="$2"
        shift
        ;;

    -MD)
        DEPFILE=1
        ;;

    -L*)
        ARG="${1#-L}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        LDIRS="$LDIRS:$ARG"
        ;;
        
    -X*)
        ARG="${1#-X}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        XDIRS="$XDIRS:$ARG"
        ;;

    --dlib)
        TYPE=dlib
        ;;

    --slib)
        TYPE=slib
        ;;

    --stub)
        TYPE=stub
        ;;

    --exe)
        TYPE=exe
        ;;

    --static)
        STATIC=1
        ;;

    --vers)
        if [ -f "$2" ]
        then
            VERSFILE="$2"
        elif [ -d "$2" ]
        then
            VERSDIR="$2"
        else
            echo "$SELF_NAME: expected version file or source directory"
            exit 3
        fi
        shift
        ;;

    -o*)
        ARG="${1#-o}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        TARG="$ARG"
        ;;

    -lz|-sz|-dz)
        LIBS="$LIBS $1"
        HAVE_GZIP=1
        ;;
    -[lds]bz2)
        LIBS="$LIBS $1"
        HAVE_BZIP=1
        ;;
    -[lds]dl)
        LIBS="$LIBS $1"
        HAVE_DL=1
        ;;

    -[lds]xml2)
        HAVE_XML=32
        ;;

    -[lds]m)
        HAVE_M=16
        ;;

    -lsradb|-ssradb|-dsradb|-lwsradb|-swsradb)
        LIBS="$LIBS $1"
        NEED_DL=1
        DYLD=2
        ;;

    -lkrypto|-dkrypto)
        LIBS="$LIBS $1"
        NEED_KPROC=1
        ;;
    -skrypto)
        LIBS="$LIBS $1"
        NEED_KPROC=1
        ;;

    -[ld]kproc)
        KPROC=4
        LIBS="$LIBS $1"
        ;;
    -skproc)
        KPROC=4
        THREADS=8
        LIBS="$LIBS $1"
        ;;

    -[lds]ncbi-vdb)
        HAVE_NCBI_VDB=1
        KPROC=4
        HAVE_GZIP=1
        HAVE_BZIP=1
        HAVE_KFC=1
        NEED_M=1
        NEED_XML=1
        LIBS="$LIBS $1"
        ;;
    -[lds]ncbi-ngs-c++)
        HAVE_NCBI_VDB=1
        KPROC=4
        HAVE_GZIP=1
        HAVE_BZIP=1
        HAVE_KFC=1
        NEED_M=1
        NEED_XML=1
        LIBS="$LIBS $1"
        ;;
    -[lds]ncbi-wvdb)
        HAVE_NCBI_WVDB=1
        KPROC=4
        HAVE_GZIP=1
        HAVE_BZIP=1
        HAVE_KFC=1
        NEED_M=16
        NEED_XML=1
        LIBS="$LIBS $1"
        ;;

    -[lds]ksproc)
        HAVE_KSPROC=1
        LIBS="$LIBS $1"
        THREADS=0
        ;;

    -[lds]pthread)
        THREADS=8
        ;;

    -[ls]kfs)
        LIBS="$LIBS $1"
        NEED_GZIP=1
        NEED_BZIP=1
        NEED_DL=1
        ;;
    -dkfs)
        LIBS="$LIBS $1"
        NEED_GZIP=1
        NEED_BZIP=1
        NEED_DL=1
        DYLD=2
        ;;

    -[ls]vfs)
        LIBS="$LIBS $1"
        ;;
    -dvfs)
        LIBS="$LIBS $1"
        DYLD=2
        ;;

    -[ls]kxml)
        LIBS="$LIBS $1"
        NEED_XML=1
        ;;
    -dkxml)
        LIBS="$LIBS $1"
        NEED_XML=1
        DYLD=2
        ;;

    -[lds]ncbi-bam)
        LIBS="$LIBS $1"
        #NEED_GZIP=1
        ;;

    -[lds]kapp)
        HAVE_KAPP=1
        ;;

    -[lds]kapp-norsrc)
        HAVE_KAPP=1
        ;;

    -[lds]kfc)
        LIBS="$LIBS $1"
        HAVE_KFC=1
        ;;

    -[ls]*)
        LIBS="$LIBS $1"
        ;;

    -d*)
        LIBS="$LIBS $1"
        DYLD=2
        ;;

    *.$OBJX)
        OBJS="$OBJS $1"
        ;;
        
    esac

    shift
done

# correct for prefixes
LDIRS="${LDIRS#:}"
XDIRS="${XDIRS#:}"
LIBS="${LIBS# }"
OBJS="${OBJS# }"

# split target
OUTDIR=$(dirname "$TARG")
NAME=$(basename "$TARG")

# dependency file
[ "$DEPFILE" != "" ] && DEPFILE="$NAME.$TYPE.d"

# parse target
if [ "$TYPE" = "dlib" ] && [ "$DYLX" != "" ]
then
    EXT="$DYLX"
    NAME="${NAME%.$DYLX}"
fi

unset VERS

V="${NAME#${NAME%\.[^.]*}\.}"
if [[ $V == ${V//[^0-9]/} ]]
then
    ARG="${NAME%\.$V}"
    VERS="$V"
    NAME="${ARG#.}"

	V="${NAME#${NAME%\.[^.]*}\.}"
	if [[ $V == ${V//[^0-9]/} ]]
    then
	    ARG="${NAME%\.$V}"
    	VERS="$V.$VERS"
	    NAME="${ARG#.}"

		V="${NAME#${NAME%\.[^.]*}\.}"
		if [[ $V == ${V//[^0-9]/} ]]
        then
		    ARG="${NAME%\.$V}"
	    	VERS="$V.$VERS"
		    NAME="${ARG#.}"
        fi
    fi
	#echo "ARG=$ARG,VERS=$VERS,NAME=$NAME"
fi

case "$TYPE" in
dlib)
    if [ "$SHLX" != "" ]
    then
        EXT="$SHLX"
        NAME="${NAME%.$SHLX}"
    fi
    ;;
slib)
    EXT="$LIBX"
    NAME="${NAME%.$LIBX}"
esac

unset DBGAP
if [ "$NAME" != "${NAME%-dbgap}" ]
then
	DBGAP=-dbgap
	NAME="${NAME%-dbgap}"
fi

# locate version file and version
[ "$VERSDIR" != "" ] && VERSFILE="$VERSDIR/$NAME.vers"
if [ "$VERSFILE" != "" ]
then
    if [ ! -f "$VERSFILE" ]
    then
        echo "$SELF_NAME: warning - creating version file '$VERSFILE'"
        echo 1.0.0 > $VERSFILE
    fi

    if [ ! -r "$VERSFILE" ]
    then
        echo "$SELF_NAME: version file '$VERSFILE' is unreadable"
        exit 5
    fi

    ARG=$(cat $VERSFILE)
    if [ "$VERS" != "" ] && [ "$VERS" != "$ARG" ] && [ "$ARG" = "${ARG#$VERS.}" ]
    then
        echo "$SELF_NAME: version from file '$VERSFILE' ($ARG) does not match '$VERS'"
        exit 5
    fi
    VERS="$ARG"
fi

# fix kapp
[ $HAVE_KAPP -ne 0 ] && [ $HAVE_KFC -ne 0 ] && LIBS="-lkapp $LIBS"
[ $HAVE_KAPP -ne 0 ] && [ $HAVE_KFC -eq 0 ] && LIBS="-lkapp-norsrc $LIBS"

# detect need for kproc
if [ $KPROC -eq 0 ] && [ $NEED_KPROC -ne 0 ] && [ $HAVE_KSPROC -eq 0 ]
then
    KPROC=4
    LIBS="$LIBS -lkproc"
fi

# turn on threads for kproc
[ $KPROC -ne 0 ] && THREADS=8

# supply missing libraries
[ $HAVE_GZIP -eq 0 ] && [ $NEED_GZIP -ne 0 ] && LIBS="$LIBS -lz"
[ $HAVE_BZIP -eq 0 ] && [ $NEED_BZIP -ne 0 ] && LIBS="$LIBS -lbz2"
[ $HAVE_DL -eq 0 ] && [ $NEED_DL -ne 0 ] && LIBS="$LIBS -ldl"
[ $HAVE_M -eq 0 ] && [ $NEED_M -ne 0 ] && HAVE_M=16
[ $HAVE_XML -eq 0 ] && [ $NEED_XML -ne 0 ] && HAVE_XML=32

# overwrite dependencies
[ -f "$DEPFILE" ] && rm -f "$DEPFILE"

# generate mode
MODE=$(expr $HAVE_XML + $HAVE_M + $THREADS + $KPROC + $DYLD + $STATIC)
#MODE=$(expr $THREADS + $KPROC + $DYLD + 1)

# generate SCM flags
SCMFLAGS=$(expr $STATICSYSLIBS + $STATICSYSLIBS + $CHECKSUM)
if [ 0 -ne 0 ]
then
    echo "# $SELF_NAME"
    echo "#   script-base    : $SCRIPT_BASE"
    echo "#   OS             : $OS"
    echo "#   type           : $TYPE"
    echo "#   tool           : $LD"
    echo "#   ARCH           : $ARCH"
    echo "#   BUILD          : $BUILD"
    echo "#   srcdir         : $SRCDIR"
    echo "#   bindir         : $BINDIR"
    echo "#   outdir         : $OUTDIR"
    echo "#   target         : $TARG"
    echo "#   name           : $NAME"
    echo "#   dbgap          : $DBGAP"
    echo "#   version        : $VERS"
    echo "#   vers file      : $VERSFILE"
    echo "#   dep file       : $DEPFILE"
    echo "#   mode           : $MODE"
    echo "#   DYLD           : $DYLD"
    echo "#   SCMFLAGS       : $SCMFLAGS"
    echo "#   LDFLAGS        : $LDFLAGS"
    echo "#   LDIRS          : $LDIRS"
    echo "#   XDIRS          : $XDIRS"
    echo "#   objects        : $OBJS"
    echo "#   libraries      : $LIBS"
    echo "#   rhost          : $RHOST"
    echo "#   rport          : $RPORT"
    echo "#   rhome          : $RHOME"
    echo "#   lhome          : $LHOME"
    echo "#   proxy_tool     : $PROXY_TOOL"

    echo "#   static sys libs: $STATICSYSLIBS"
    echo "#   checksum       : $CHECKSUM"
    echo "#   static         : $STATIC"
    echo "#   kproc          : $KPROC"
    echo "#   thread libs    : $THREADS"
    echo "#   vers dir       : $VERSDIR"
    echo "#   extension      : $EXT"
fi

# perform link
"$SCRIPT_BASE.$OS.$TYPE.sh" "$LD" "$ARCH" "$BUILD" "$SRCDIR" "$BINDIR" "$OUTDIR" \
    "$TARG" "$NAME" "$DBGAP" "$VERS" "$VERSFILE" "$DEPFILE" "$MODE" "$SCMFLAGS" \
    "$LDFLAGS" "$LDIRS" "$XDIRS" "$OBJS" "$LIBS" "$PROXY_TOOL" "$RHOST" "$RPORT" "$RHOME" "$LHOME" "$(pwd)" "$ROUTDIR" "$LOUTDIR"  || exit $?

# establish links
if [ "$VERS" != "" ] && [ "$OS" != "win" ] && [ "$OS" != "rwin" ]
then
    $SCRIPT_BASE.$OS.ln.sh "$TYPE" "$OUTDIR" "$TARG" "$NAME" "$DBGAP" "$EXT" "$VERS"
fi

# SCM
if [ $CHECKSUM -eq 1 ] && [ "$OS" = "linux" ]
then
    # calling the scm-version-script
    # parameters are: module-name, current-md5-file, version-file
    if [ $TYPE = "dlib" ] || [ $TYPE = "exe" ] || [ $STATIC -eq 1 ]
    then
        SCM_DIR="$TOP/scm"
        LOGFILE="$SCM_DIR/scm.log"
        SCMD="$BUILD_DIR/scm.sh $NAME $TARG.md5 $VERSFILE"
        echo "$SCMD" >> $LOGFILE
        $SCMD
    fi
fi
