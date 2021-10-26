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


# ===========================================================================
# input library types, and their handling
#
#  normal or static linkage
#   -l : require static
#   -s : require static
#   -d : ignore
# ===========================================================================

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
        mv $m $LIBNAME-$m
        CMD="$CMD ld-tmp/$LIBNAME-$m"
    done

    # return to prior location
    cd - > /dev/null
}

# versioned output
if [ "$VERS" = "" ]
then
    CMD="$CMD $TARG"
else
    set-vers $(echo $VERS | tr '.' ' ')
    CMD="$CMD $OUTDIR/$NAME$DBGAP.a.$VERS"
fi

# tack on object files
CMD="$CMD $OBJS"

# initial dependency upon Makefile and vers file
DEPS="$SRCDIR/Makefile"
if [ "$LIBS" != "" ]
then
    # tack on libraries, finding as we go
    for LIB in $LIBS
    do

        # strip off switch
        LIBNAME="${LIB#-[lsd]}"

        # look at linkage
        case "$LIB" in
        -l*|-s*)

            # force static load
            LIBPATH="$(find-lib $LIBNAME.a $LDIRS)"
            if [ "$LIBPATH" != "" ]
            then

                # add it to dependencies
                DEPS="$DEPS $LIBPATH"

                # convert to individual object files
                convert-static "$LIBPATH" || exit $?

            fi
            ;;

        esac

    done
fi

# produce static library
echo $CMD
$CMD || exit $?

# remove temporaries
rm -rf ld-tmp

# produce dependencies
if [ "$DEPFILE" != "" ] && [ "$DEPS" != "" ]
then
    echo "$TARG: $DEPS" > "$DEPFILE"
fi
