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
# input library types, and their handling
#
#  normal or static linkage
#   -l : require static
#   -s : require static
#   -d : ignore
# ===========================================================================

# script name
SELF_NAME="$(basename $0)"

# parameters and common functions
source "${0%rwin.slib.sh}win.cmn.sh"

# initialize command
CMD="ar -rc"

# function to convert an archive into individual object files
convert-static ()
{
    # list members
    local path="$1"
    local mbrs="$(ar -t $path)"

    # unpack archive into temporary directory
    mkdir -p ld-tmp
    if ! cd ld-tmp
    then
        echo "$SELF_NAME: failed to cd to ld-tmp"
        exit 5
    fi
    ar -x "$path"

    # rename and add to source files list
    local m=
    for m in $mbrs
    do
        mv $m $xLIBNAME-$m
        CMD="$CMD ld-tmp/$xLIBNAME-$m"
    done

    # return to prior location
    cd - > /dev/null
}

CMD="$CMD $TARG $OBJS"

# initial dependency upon Makefile and vers file
DEPS="$SRCDIR/Makefile"
if [ "$LIBS" != "" ]
then
    # tack on libraries, finding as we go
    for xLIB in $LIBS
    do
        # strip off switch
        xLIBNAME="${xLIB#-[lsd]}"

        # look at linkage
        case "$xLIB" in
        -s*)

            # force static load
            find-lib $xLIBNAME.a $LDIRS # .a for static, .lib for dynamic?
            if [ "$xLIBPATH" != "" ]
            then

                # add it to dependencies
                DEPS="$DEPS $xLIBPATH"

                # convert to individual object files
                convert-static "$xLIBPATH" || exit $?

            fi
            ;;

        esac
    done
fi

# produce static library
rm -f $TARG
echo $CMD

$CMD || exit $?

# remove temporaries
rm -rf ld-tmp

# produce dependencies
if [ "$DEPFILE" != "" ] && [ "$DEPS" != "" ]
then
    echo "$TARG: $DEPS" > "$DEPFILE"
fi

exit $STATUS
