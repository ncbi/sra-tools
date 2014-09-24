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

# script name
SELF_NAME="$(basename $0)"

# parameters
LD="$1"
shift

SRCDIR="$1"
OUTDIR="$2"
TARG="$3"
NAME="$4"
DBGAP="$5"
shift 5

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

# decode MODE
STATIC=$(expr $MODE % 2)
DYLD=$(expr $MODE / 2)

# decode SCMFLAGS
CHECKSUM=$(expr $SCMFLAGS % 2)
STATICSYSLIBS=$(expr $SCMFLAGS / 2)

# return parameter for find-lib
LIBPATH=''

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

    LIBPATH=''

    while [ "$_dirs" != "" ]
    do
        _dir="${_dirs%%:*}"

        if [ "$_dir" != "" ]
        then
            if [ -e "$_dir/$_lib" ]
            then
                while [ -L "$_dir/$_lib" ]
                do
                    _lib=$(stat -c '%N' "$_dir/$_lib" | tr "\`\'" "  ")
                    _lib="${_lib#*->}"
                    _lib="lib${_lib# *lib}"
                    _lib="${_lib%% *}"
                done
                LIBPATH="$_dir/$_lib"
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
