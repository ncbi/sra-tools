#!/bin/sh
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

#
# Update an SRA archive (.sra) with fingerprinting information
#
# Expects PATH to contain sra-tools' binaries (kar, sra-add-fp, kdbmeta)
#
# $1 - path to an SRA archive
# $2 - if not empty, print the inserted metadata
#
# return codes:
# 0 - passed
# 1 - the archive is not found
# 2 - kar -x failed
# 3 - sra-add-fp failed
# 4 - kar -c failed
# 5 - kdbmeta failed

TARGET=$1
VERBOSE=$2

TEMP=/tmp/$(id -u)/sra-add-fp
mkdir -p $TEMP

if [ ! -f $TARGET ]; then
    echo "the target archive $TARGET is not found"
    exit 1
fi

run_cmd() {
    CMD="$1"
    eval $CMD
    rc="$?"
    if [ "$rc" != "0" ] ; then
        echo $CMD
        echo "returned $rc"
        exit $2
    fi
}

run_cmd "kar -x $TARGET -d $TEMP" 2
run_cmd "sra-add-fp $TEMP" 3
run_cmd "kar -f -c $TARGET -d $TEMP" 4

if [ "$VERBOSE" != "" ] ; then
    run_cmd "kdbmeta -X t $TARGET -T SEQUENCE QC/current" 5
fi

rm -rf $TEMP