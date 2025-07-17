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

BINARY_PATH="$1"
HOST_OS="$2"
BUILD_TYPE="$3"

# FIXME: Is this necessary?  Why not just use kill -s QUIT or kill -QUIT?
SIGQUIT=3

function killFromBackground ()
{
    PARENT_PID=$1
    for I in 1 2 3 4 5
    do
        if [ "$HOST_OS" = "bsd" ]; then
            COMMAND="ps awwo pid,ppid,command|awk '\$2==${PARENT_PID} && \$3==\"${BINARY_PATH}\"  {print \$1}'"
        else
            COMMAND="ps -ef|awk '\$3==${PARENT_PID} && \$8==\"${BINARY_PATH}\"  {print \$2}'"
        fi
        eval PID=\`${COMMAND}\`
        if [ "$PID" = "" ]; then
            sleep 1
        else
            break
        fi
    done

    if [ "$PID" != "" ]; then
        kill -s $SIGQUIT $PID
    fi

    echo $PID > killed.pid
}

# Check the environment if core files can be generated
if [ "$HOST_OS" = "mac" ]; then
   if [ "`ulimit -c`" = "0" ] || [ "`/usr/sbin/sysctl -n kern.coredump`" != "1" ]; then
       echo "Core files are disabled. Skipping core file tests"
       exit 0
   fi

   if [ ! -d "/core" ]; then
        echo "/core folder is missing - core files are disabled. Skipping core files tests"
        exit 0
   fi

   CORE_FOLDER="/core/"
elif [ "$HOST_OS" = "bsd" ]; then
   if [ "`ulimit -c`" = "0" ] || [ "`/sbin/sysctl -n kern.coredump`" != "1" ]; then
       echo "Core files are disabled. Skipping core file tests"
       exit 0
   fi

   CORE_FOLDER="./"
elif [ "$HOST_OS" = "linux" ]; then
    if [ "$OS_DISTRIBUTOR" = "Ubuntu" ]; then
        if [ "`ulimit -c`" = "0" ]; then
            echo "Core files are disabled. Skipping core file tests"
            exit 0
        fi

        if [ "`cat /proc/sys/kernel/core_pattern`" != "core" ]; then
            echo "Unknown core file pattern. Skipping core file tests" 1>&2
            exit 0
        fi
    else
        echo "Non-Ubuntu Linux. Skipping core file tests"
        exit 0
    fi
   CORE_FOLDER="./"
else
   echo "Should be run from unix-compatible OS" 1>&2
   exit 1
fi

# Bash overrides signals in child process if strarting
# them in background. Lets start it normally and run
# `kill` command in background.
killFromBackground $$ &
$BINARY_PATH

# Wait for `kill` job
wait

# Extract PID of binary that we killed
BINARY_PID=`cat killed.pid`
rm killed.pid

if [ "$HOST_OS" = "bsd" ]; then
    CORE_FILE="${CORE_FOLDER}${BINARY_PATH##*/}.core"
else
    CORE_FILE="${CORE_FOLDER}core"
fi

if [ "$BUILD_TYPE" = "dbg" ]; then
   if [ -f $CORE_FILE ]; then
       rm $CORE_FILE
       echo "Success: Core file $CORE_FILE was generated and was removed"
       exit 0
   else
       echo "Failed: No core file $CORE_FILE detected" 1>&2
       exit 3
   fi
else
   if [ -f $CORE_FILE ]; then
       rm $CORE_FILE
       echo "Failed: Core file $CORE_FILE generated while shouldn't and was removed" 1>&2
       exit 4
    else
       echo "Success: No core file $CORE_FILE detected"
       exit 0
    fi
fi

