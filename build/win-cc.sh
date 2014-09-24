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
# prepare script name
SELF_NAME="$(basename $0)"
SCRIPT_BASE="${0%.sh}"

# os
OS="$1"
shift

# binary compiler
CC="$1"
shift

# configuration
unset TARG
unset ARGS
unset OBJX
unset SRCFILE
unset SOURCES
unset DEPENDENCIES
# paths for translating local to remote
unset RHOME
unset LHOME
unset ROUTDIR
unset RHOST
unset RPORT
unset PROXY_TOOL

function convert_path
{
    if [ "$OS" != "rwin" ] ; then
        convert_path_result="$(cygpath -w $1)"
    elif [ "$1" != "." ] ; then
        if [ "${1#$LHOME}" != "$1" ] ; then
            convert_path_result="$RHOME${1#$LHOME}"
        else
            convert_path_result="$1"
        fi
        convert_path_result="${convert_path_result//\//\\}"
    else
        convert_path_result="."
    fi
}

while [ $# -ne 0 ]
do

    case "$1" in
    --cflags)
        for ARG in $2
        do
            [ "$ARG" != "${ARG#-}" ] && ARG="/${ARG#-}"
            ARGS="$ARGS $ARG"
        done
        shift
        ;;

    --checksum)
        ;;

    --objx)
        OBJX="$2"
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

    -D*)
        ARG="${1#-D}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        ARGS="$ARGS /D$ARG"
        ;;

    -I*)
        ARG="${1#-I}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        convert_path $ARG
        ARGS="$ARGS /I$convert_path_result"
        ;;

    -o*)
        ARG="${1#-o}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        ARGS="$ARGS /Fo$ARG"
        TARG="${ARG%.$OBJX}"
        ;;

    -MD)
        # the /showIncludes switch will generate
        # includes to stderr, but they will need
        # to be filtered out and rewritten to the *.d
        ARGS="$ARGS /showIncludes"
        DEPENDENCIES=1
        ;;

    -fPIC|-std=c99|-ansi)
        ;;
        
    -pedantic)
        ARGS="$ARGS /W3"
        ;;

    -*)
        ARG="/${1#-}"
        ARGS="$ARGS $ARG"
        ;;

    *)
        SRCFILE="$(basename $1)"
        SOURCES="$SOURCES $1"
        convert_path $1
        ARGS="$ARGS $convert_path_result"
        SRCDIR="$(dirname $1)" # assume the last source file is in the same dir as all the .vers files we need as dependencies
        ;;
        
    esac

    shift
done

unset STATUS

CMD="$CC $ARGS"
if [ "$VERBOSE" != "" ] ; then echo "CMD=$CMD"; fi

# run command with redirection
if [ "$OS" = "win" ]
then
    $CMD > $TARG.out 2> $TARG.err
    STATUS=$?
else
    # determine current directory
    CURDIR="$(pwd)"
	if [ "$VERBOSE" != "" ] ; then echo "${TOP}/build/run_remotely.sh $PROXY_TOOL $RHOST $RPORT $RHOME $LHOME $CURDIR $ROUTDIR $LOUTDIR $CMD"; fi
    ${TOP}/build/run_remotely.sh $PROXY_TOOL $RHOST $RPORT $RHOME $LHOME $CURDIR $ROUTDIR $LOUTDIR $CMD > $TARG.out 2> $TARG.err
    STATUS=$?
fi

# check for dependencies
if [ "$DEPENDENCIES" = "1" ]
then
    sed -e '/including file/!d' -e 's/.*including file: *\([^\r\n][^\r\n]*\)/\1/g' -e '/ /d' $TARG.out > $TARG.inc
    echo -n "$TARG.$OBJX: $SOURCES" | sed -e 's/\r//g' > $TARG.d
    if [ "$OS" = "win" ]
    then 
        for inc in $(cat $TARG.inc)
        do
            # vers.h files are now generated in the objdir
            if [ "$inc" != "${inc%.vers.h}" ]
            then
                inc="${inc%.h}"
            fi
            echo -n " $(cygpath -u $inc)"  | sed -e 's/\r//g' >> $TARG.d
        done
    else # rwin
        # sometimes home path comes back in lowercase
        # make sure to compare lowercased prefixes
        rhome_low=$(echo ${RHOME} | tr '[A-Z]' '[a-z]' | tr '\\' '/')
        rhome_len=${#RHOME}
        routdir_low=$(echo ${ROUTDIR} | tr '[A-Z]' '[a-z]' | tr '\\' '/')
        routdir_len=${#ROUTDIR}

        for inc in $(cat $TARG.inc)
        do
            inc_low=$(echo ${inc} | tr '[A-Z]' '[a-z]' | tr '\\' '/')
            if [ "${inc_low#$rhome_low}" != "$inc_low" ]
            then
                inc="$LHOME${inc:$rhome_len}"
            elif [ "${inc_low#$routdir_low}" != "$inc_low" ]
            then
                inc="$OUTDIR${inc:$routdir_len}"
            fi
            echo -n " $inc" | tr '\\' '/' >> $TARG.d
        done
    fi
    echo >> $TARG.d
fi

echo "$CMD"
# repeat output files but without CR
sed -e 's/\r//g' $TARG.err > /dev/stderr
sed -e 's/\r//g' -e "/^$SRCFILE$/d" -e '/including file/d' $TARG.out

# clean up files
rm -f $TARG.out $TARG.err $TARG.inc

exit $STATUS
