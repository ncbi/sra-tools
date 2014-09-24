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

# check for '-F' switch
if [ "$1" = "-F" ]
then
    CVSTAG="cvs tag $1"
    shift 1
else
    CVSTAG="cvs tag"
fi

# check for '-V' switch
if [ "$1" = "-V" ]
then
    VERSFILE="$2"
    shift 2
else
    VERSFILE=""
fi

# gather parameters
TOP=$(dirname $0)
cd $TOP/..
TOP=$(pwd)
MODULE=$1
LIBRARY=$2
shift 2

# version file
if [ "$VERSFILE" = "" ]
then
    if [ ! -r $MODULE/$LIBRARY.vers ]
    then
        echo "cannot access $MODULE/$LIBRARY.vers"
        exit 1
    fi

    VERSFILE="$MODULE/$LIBRARY.vers"
fi

# version
VERSION=$(cat $VERSFILE)

# build parameters
BUILD=$(cat ./build/BUILD)
COMP=$(cat ./build/COMP)
OUTDIR=$(cat ./build/OUTDIR)
cd $OUTDIR
OUTDIR=$(pwd)
cd -

# architecture
ARCH=$(uname -m)
if [ "$ARCH" = "i486" ] || [ "$ARCH" = "i586" ] || [ "$ARCH" = "i686" ]
then
    ARCH=i386
fi

# object directory
OBJDIR=$OUTDIR/$BUILD/$COMP/$ARCH/obj/$MODULE
if [ ! -d $OBJDIR ]
then
    echo "cannot access $OBJDIR"
    exit 1
fi

# tag name
TAGVERS=$(echo $VERSION | tr '.' '_')
if [ "$BUILD" = "rel" ]
then
    TAG="$LIBRARY-$TAGVERS"
else
    TAG="$BUILD-$LIBRARY-$TAGVERS"
fi

# include make files in sources
SOURCES="$(find build -name 'Makefile*' -a ! -name '*~')"
if [ -f $MODULE/Makefile ]
then
    SOURCES="$SOURCES $MODULE/Makefile"
fi

# also include the version file if within the module
if [ "$VERSFILE" = "$MODULE/$LIBRARY.vers" ]
then
    SOURCES="$SOURCES $VERSFILE"
fi

# allow direct specification of source files
if [ "$1" = "-S" ]
then
    shift

    # take list of sources as given
    for sfile in $*
    do

        # source file must be within our tree
        if [ "$sfile" = "${sfile#$TOP}" ]
        then

            # get path portion of source file
            spath=$(dirname $sfile)

            # look for absolute or project relative
            if [ -f "$sfile" ]
            then
                cd "$spath"
                spath="$(pwd)"
                cd -

            # look for module relative
            elif [ -f "$MODULE/$sfile" ]
            then
                cd "$MODULE/$spath"
                spath="$(pwd)"
                cd -
            fi

            # retest path
            if [ "$spath" = "${spath#$TOP}" ]
            then
                echo "source file '$sfile' is not within project"
                exit 1
            fi

            # create full path
            sfile="$spath/$(basename $sfile)"
        fi

        # crop it
        sfile="${sfile#$TOP/}"
        SOURCES="$SOURCES $sfile"

    done

else

    # derive source files from dependency files
    for ofile in $*
    do
        # create dependency file name
        dfile=${ofile%o}d

        # test for it
        if [ ! -r $OBJDIR/$dfile ]
        then
            echo "cannot locate dependency file '$OBJDIR/$dfile'"
            exit 1
        fi

        # read it
        for sfile in $(cat $OBJDIR/$dfile)
        do
            # source file must be within our tree
            # and should not be a generated version include
            if [ "$sfile" != "${sfile#$TOP}" ] && [ "$sfile" = "${sfile%.vers.h}" ]
            then
                # crop it
                sfile="${sfile#$TOP/}"

                # special case include files in 'inc'
                # in order to catch all configurations
                if [ "$sfile" != "${sfile#inc/}" ]
                then
                    sfile=$(basename $sfile)
                    sfile="$(find inc -name $sfile)"
                fi
            
                SOURCES="$SOURCES $sfile"
            fi

        done

    done
fi

# must have source files
if [ "$SOURCES" = "" ]
then
    echo "no source files could be found for $LIBRARY"
    exit 1
fi

# reduce them to an unique set
SOURCES="$(echo "$SOURCES" | tr ' ' '\n' | sort -u | tr '\n' ' ')"

# apply tag
echo -e "\ntagging $TAG within $MODULE"
$CVSTAG $TAG $SOURCES
