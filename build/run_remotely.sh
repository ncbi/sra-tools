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

# ===========================================================================
# run a command from a list on a (Windows) build server
# $1 - path to proxy_exec
# $2 - server name 
# $3 - server port
# $4 - server path mapped to a client dir (e.g. Z:)
# $5 - client path to the dir mapped on the server
# $6 - server path to the working directory (e.g. Z:internal\asm-trace)
# $7 - output directory from server's prespective (e.g. Z:\win\lib)
# $8 - output directory from client's perspective (e.g. internal/asm-trace/win/lib)
# $9 - command (can be a single name e.g. cl, or a client path which will be translated into a server path) 
# $... - arguments

#TODO: translate command name to id

PROXY_TOOL=$1
if [ ! -e $PROXY_TOOL ]
then
    echo "$0 $*"
    echo "$0: proxy tool ($1) is not found."
    exit 5
fi

RHOST=$2
RPORT=$3
RHOME=$4
LHOME=$5
WORKDIR=$6
ROUTDIR=$7
LOUTDIR=$8
CMD=$9
shift 9
ARGS=$*

if [ "$VERBOSE" != "" ] 
then 
    echo "PROXY_TOOL=$PROXY_TOOL"
    echo "RHOST     =$RHOST     "
    echo "RPORT     =$RPORT     "
    echo "RHOME     =$RHOME     "
    echo "LHOME     =$LHOME     "
    echo "WORKDIR   =$WORKDIR   "
    echo "ROUTDIR   =$ROUTDIR   "
    echo "LOUTDIR   =$LOUTDIR   "
    echo "CMD       =$CMD       "
    echo "ARGS      =$ARGS      "
fi

if [ $(eval "dirname \"$CMD\"") != "." ] 
then # executable is located on the client; translate path for the server
    if [ "$CMD" != "${CMD#$LHOME}" ] 
    then
        RCMD="$RHOME${CMD#$LHOME}.exe"
    elif [ "$CMD" != "${CMD#$LOUTDIR}" ] 
    then
        RCMD="$ROUTDIR${CMD#$LOUTDIR}.exe"
    else
        RCMD="$CMD.exe"
    fi

    if [ "${WORKDIR}" == "." ] 
    then # run in the directory of the executable
        # use the original CMD
        WORKDIR="${CMD%$(basename $CMD)}"
    fi
else # executable is located on the server
    # workdir path translation is expected of the caller
    RCMD=$CMD
fi

# translate WORKDIR
if [ "$WORKDIR" != "${WORKDIR#$LHOME}" ] 
then
    WORKDIR="$RHOME${WORKDIR#$LHOME}"
elif [ "$WORKDIR" != "${WORKDIR#$LOUTDIR}" ] 
then
    WORKDIR="$ROUTDIR${WORKDIR#$LOUTDIR}"
fi

if [ "$VERBOSE" != "" ] ; then echo "RCMD=$RCMD"; fi

#translate slashes
RCMD="$(echo $RCMD | tr '/' '\\')"
WORKDIR="$(echo $WORKDIR | tr '/' '\\')"

#extra quotes in case command's filename contains spaces
if [[ "$RCMD" != "${RCMD/ /}" ]] ; then RCMD="\"$RCMD\""; fi

if [ "$VERBOSE" != "" ] ; then echo "$PROXY_TOOL -D $WORKDIR -S $RHOST -P $RPORT"; fi
if [ "$VERBOSE" != "" ] ; then echo "sending to stdin: $RCMD $ARGS"; fi

echo "$RCMD $ARGS" | $PROXY_TOOL -D $WORKDIR -S $RHOST -P $RPORT
exit $?




