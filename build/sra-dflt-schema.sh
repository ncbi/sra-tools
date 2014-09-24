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
SCRIPT_BASE="${0%.sh}"

# os
OS="$1"
shift

# binary compiler
CC="$1"
shift 

# everything except windows is fine as-is
if [[ "$OS" != "win" && "$OS" != "rwin" ]]
then 
    echo "$CC $*"
    $CC $*
    exit $?
fi

if [[ "$OS" == "rwin" ]]
then # for rwin, $CC is a multi-word string; extract server location data from it:
#                $(TOP)/build/run_remotely.sh $(PROXY_TOOL) $(RHOST) $(RPORT) $(RHOME) $(LHOME) $(TOP) $(ROUTDIR) $(LOUTDIR) $(SCHEMA_EXE)
    PARMS=($CC)
    RHOME=${PARMS[4]}
    LHOME=${PARMS[5]}
    ROUTDIR=${PARMS[7]}
    LOUTDIR=${PARMS[8]}
fi

# state
unset ARGS
unset DEPENDENCIES
unset DEPTARG
unset TARG

function convert_src_path
{
    if [[ "$OS" != "rwin" ]]
    then
        convert_path_result="$(cygpath -w $1)"
    elif [ "$1" != "${1#$LHOME}" ] 
    then
        convert_path_result="$RHOME${1#$LHOME}"
        convert_path_result="$(echo $convert_path_result | tr '/' '\\')"
    else
        convert_path_result="$1"
    fi
}
function convert_out_path
{
    if [[ "$OS" != "rwin" ]]
    then
        convert_path_result="$(cygpath -w $1)"
    elif [ "$1" != "${1#$LOUTDIR}" ] 
    then
        convert_path_result="$ROUTDIR${1#$LOUTDIR}"
        convert_path_result="$(echo $convert_path_result | tr '/' '\\')"
    else
        convert_path_result="$1"
    fi
}


# process parameters for windows
while [ $# -ne 0 ]
do

    case "$1" in
    -o*)
        ARG="${1#-o}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        TARG="$ARG"
        convert_out_path $ARG
        ARGS="$ARGS -o${convert_path_result}"
        ;;

    -I*)
        ARG="${1#-I}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        
        convert_src_path $ARG
        ARGS="$ARGS -I${convert_path_result}"
        ;;

    -T*)
        ARG="${1#-T}"
        if [ "$ARG" = "" ]
        then
            ARG="$2"
            shift
        fi
        DEPTARG="$ARG"
        DEPENDENCIES=1
        convert_out_path $ARG
        ARGS="$ARGS -T${convert_path_result}"
        ;;

    -L*)
        LHOME="${1#-L}"
        ;;
    -R*)
        RHOME="${1#-R}"
        ;;
    
    *)
        convert_src_path $1
        ARGS="$ARGS ${convert_path_result}"
        ;;

    esac

    shift

done

echo "$CC $ARGS"
$CC $ARGS
STATUS=$?
if [[ ${STATUS} != 0 ]]
then
    rm -f $TARG $DEPTARG
    exit ${STATUS}
fi

if [ $DEPENDENCIES -eq 1 ]
then
    # fix this
    rm -f $DEPTARG
fi
