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
VERBOSE=
if [ "$VERBOSE" != "" ] ; then echo "$0 $*"; fi

# script name
SELF_NAME="$(basename $0)"

# parameters
LD="$1"
ARCH="$2"
BUILD="$3"
shift 3

SRCDIR="$1"
BINDIR="$2"
OUTDIR="$3"
TARG="$4"
NAME="$5"
DBGAP="$6"
shift 6

VERS="$1"
VERSFILE="$2"
DEPFILE="$3"
shift 3

MODE="$1"
SCMFLAGS="$2"
LDFLAGS="$3"
shift 3

LDIRS="$1"
XDIRS="$2"
shift 2

OBJS="$1"
LIBS="$2"
shift 2

PROXY_TOOL=$1
RHOST=$2
RPORT=$3
RHOME=$4
LHOME=$5
WORKDIR=$6
ROUTDIR=$7
LOUTDIR=$8
shift 8

# decode MODE
STATIC=$(expr $MODE % 2)
MODE=$(expr $MODE / 2)
DYLD=$(expr $MODE % 2)
MODE=$(expr $MODE / 2)
KPROC=$(expr $MODE % 2)
MODE=$(expr $MODE / 2)
THREADS=$(expr $MODE % 2)
MODE=$(expr $MODE / 2)
HAVE_M=$(expr $MODE % 2)
MODE=$(expr $MODE / 2)
HAVE_XML=$(expr $MODE % 2)

##### TEMPORARY #####
KPROC=0
THREADS=0
HAVE_M=0
HAVE_XML=0
#####################

# decode SCMFLAGS
CHECKSUM=$(expr $SCMFLAGS % 2)
STATICSYSLIBS=$(expr $SCMFLAGS / 2)

# set target architecture
if [ "$ARCH" == "i386" ]
then
    LDFLAGS="$LDFLAGS /MACHINE:x86"
else
    LDFLAGS="$LDFLAGS /MACHINE:x64"
fi

if [ "$VERBOSE" != "" ] 
then
echo "LD            =$LD"
echo "ARCH          =$ARCH"
echo "BUILD         =$BUILD"
echo "SRCDIR        =$SRCDIR"        
echo "BINDIR        =$BINDIR"        
echo "OUTDIR        =$OUTDIR"        
echo "TARG          =$TARG"          
echo "NAME          =$NAME"          
echo "DBGAP         =$DBGAP"         
echo "VERS          =$VERS"          
echo "VERSFILE      =$VERSFILE"      
echo "DEPFILE       =$DEPFILE"       
echo "MODE          =$MODE"          
echo "SCMFLAGS      =$SCMFLAGS"      
echo "LDFLAGS       =$LDFLAGS"       
echo "LDIRS         =$LDIRS"         
echo "XDIRS         =$XDIRS"         
echo "OBJS          =$OBJS"          
echo "LIBS          =$LIBS"   
echo "PROXY_TOOL    =$PROXY_TOOL"   
echo "RHOST         =$RHOST"   
echo "RPORT         =$RPORT"   
echo "RHOME         =$RHOME"   
echo "LHOME         =$LHOME"   
echo "WORKDIR       =$WORKDIR" 
echo "ROUTDIR       =$ROUTDIR" 
echo "LOUTDIR       =$LOUTDIR"       
echo "STATIC        =$STATIC"        
echo "MODE          =$MODE"          
echo "DYLD          =$DYLD"          
echo "MODE          =$MODE"          
echo "KPROC         =$KPROC"         
echo "MODE          =$MODE"          
echo "THREADS       =$THREADS"       
echo "MODE          =$MODE"          
echo "HAVE_M        =$HAVE_M"        
echo "HAVE_XML      =$HAVE_XML"        
echo "CHECKSUM      =$CHECKSUM"      
echo "STATICSYSLIBS =$STATICSYSLIBS" 
fi

# return parameter for find-lib
xLIBPATH=''

# initial command state
CMD=''
LD_STATIC_STATE=0
LD_ALL_STATE=0

# for breaking out version
set-vers ()
{
    MAJ=$1
    MIN=$2
    REL=$3
}

# for locating libraries
find-lib ()
{
    _lib="lib$1"
    _dirs="$2"

    xLIBPATH=''

    while [ "$_dirs" != "" ]
    do
        _dir="${_dirs%%:*}"

        if [ "$_dir" != "" ]
        then
            if [ -e "$_dir/$_lib" ]
            then
                xLIBPATH="$_dir/$_lib"
                break;
            fi
        fi

        _dirs="${_dirs#$_dir}"
        _dirs="${_dirs#:}"
    done
}

# setting state
load-static ()
{
    if [ $LD_STATIC_STATE -eq 0 ]
    then
        CMD="$CMD $LD_STATIC"
        LD_STATIC_STATE=1
    fi
}

load-dynamic ()
{
    if [ $LD_STATIC_STATE -eq 1 ]
    then
        CMD="$CMD $LD_DYNAMIC"
        LD_STATIC_STATE=0
    fi
}

load-all-symbols ()
{
    if [ $LD_ALL_STATE -eq 0 ]
    then
        CMD="$CMD $LD_ALL_SYMBOLS"
        LD_ALL_STATE=1
    fi
}

load-ref-symbols ()
{
    if [ $LD_ALL_STATE -eq 1 ]
    then
        CMD="$CMD $LD_REF_SYMBOLS"
        LD_ALL_STATE=0
    fi
}
