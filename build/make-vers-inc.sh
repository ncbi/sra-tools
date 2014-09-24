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


# capture input
if [ $# -eq 1 ]
then
    VERSION=$1
    DEFINE=$(basename $VERSION)
    DEFINE=$(echo $DEFINE | tr '[a-z].-' '[A-Z]__')
elif [ $# -ne 2 ]
then
    echo "#error in makefile"
    exit 1
else
    DEFINE=$1
    VERSION=$2
fi

# split the version into separate integers
SPLIT_VERS="$(cat $VERSION | tr '.' ' ')"

# issue a single line define
echo "#define $DEFINE $(printf 0x%02X%02X%04X $SPLIT_VERS)"
