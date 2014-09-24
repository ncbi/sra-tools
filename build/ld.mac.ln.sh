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
TYPE="$1"
OUTDIR="$2"
TARG="$3"
NAME="$4"
DBGAP="$5"
EXT="$6"
VERS="$7"

# find target
TARG=$(basename "$TARG")

# put extension back onto name, unless it's "dylib"
NAME="$NAME$DBGAP"
STATIC_LIB_NAME="$NAME-static.a"
[ "$EXT" != "" ] && [ "$EXT" != "dylib" ] && NAME="$NAME.$EXT"

# break out version
set-vers ()
{
    MAJ=$1
    MIN=$2
    REL=$3
}

set-vers $(echo $VERS | tr '.' ' ')

cd "$OUTDIR" || exit 5

# assemble versioned names
NAME_MMR="$NAME.$MAJ.$MIN.$REL"
NAME_MM="$NAME.$MAJ.$MIN"
NAME_M="$NAME.$MAJ"

# if extension was "dylib", NOW append to names
if [ "$EXT" = "dylib" ]
then
    NAME_MMR="$NAME_MMR.$EXT"
    NAME_MM="$NAME_MM.$EXT"
    NAME_M="$NAME_M.$EXT"
    NAME="$NAME.$EXT"
fi

# create link
create-link ()
{
    rm -f "$2"
    local CMD="ln -s $1 $2"
    echo $CMD
    $CMD
}

# test for version in target name
if [ "$TARG" != "$NAME_MMR" ]
then

    # for simple name, create 2 links
    if [ "$TARG" = "$NAME" ]
    then
        create-link "$NAME_MMR" "$NAME_M"
        create-link "$NAME_M" "$NAME"
        [ "$EXT" = "a" ] && create-link "$NAME" "$STATIC_LIB_NAME"

    # for name with major version in it
    elif [ "$TARG" = "$NAME_M" ]
    then
        create-link "$NAME_MMR" "$NAME_M"


    # for name with major & minor version in it
    elif [ "$TARG" = "$NAME_MM" ]
    then
        create-link "$NAME_MMR" "$NAME_MM"
    fi
fi
