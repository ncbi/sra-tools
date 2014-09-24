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
#  normal linkage
#   -l : find shared or static
#   -s : require static
#   -d : ignore - will be dynamically loaded
#
#  static linkage
#   -l : require static
#   -s : require static
#   -d : require static
# ===========================================================================


# script name
SELF_NAME="$(basename $0)"
BUILD_DIR="$(dirname $0)"

# parameters and common functions
source "${0%exe.sh}cmn.sh"

# discover tool chain
case "$LD" in
cc)
    source "${0%exe.sh}cc.sh"
    ;;
gcc)
    source "${0%sun.exe.sh}linux.gcc.sh"
    ;;
*)
    echo "$SELF_NAME: unrecognized ld tool - '$LD'"
    exit 5
esac

# EXE_CMD was started in tool-specific source
CMD="$EXE_CMD $LDFLAGS"

# if building a static executable against dynamic libraries
# the main application will substitute for name lookup
if [ $STATIC -eq 1 ] && [ $DYLD -eq 1 ]
then
#    CMD="$CMD $LD_EXPORT_GLOBAL $LD_MULTIPLE_DEFS"
    CMD="$CMD $LD_EXPORT_GLOBAL"
fi

# tack on object files
CMD="$CMD $OBJS"

# list of static libraries used to create executable
SLIBS=''

# initial dependency upon Makefile and vers file
DEPS="$SRCDIR/Makefile $VERSFILE"
if [ "$LIBS" != "" ]
then
    # tack on paths
    DIRS="$LDIRS:$XDIRS"
    while [ "$DIRS" != "" ]
    do
        DIR="${DIRS%%:*}"
        [ "$DIR" != "" ] && CMD="$CMD -L$DIR"
        DIRS="${DIRS#$DIR}"
        DIRS="${DIRS#:}"
    done

    # update LD_LIBRARY_PATH
    unset LD_LIBRARY_PATH
    export LD_LIBRARY_PATH="$LDIRS:$XDIRS"

    # tack on libraries, finding as we go
    for LIB in $LIBS
    do

        # strip off switch
        LIBNAME="${LIB#-[lsd]}"

        # look at linkage
        case "$LIB" in
        -ldl|-ddl)

            # always load libdl as shared library
            load-ref-symbols
            load-dynamic
            CMD="$CMD -ldl"
            ;;

        -l*)

            # normal or dynamic linkage
            FOUND=0
            if [ $STATIC -eq 0 ]
            then
                find-lib $LIBNAME.so $LDIRS
                if [ "$LIBPATH" != "" ]
                then

                    # found it
                    FOUND=1

                    # load dynamic
                    load-dynamic
                    CMD="$CMD -l$LIBNAME"

                fi
            fi

            # try static only
            if [ $FOUND -eq 0 ]
            then
                find-lib $LIBNAME.a $LDIRS
                if [ "$LIBPATH" != "" ]
                then

                    # found it
                    FOUND=1

                    # add it to dependencies
                    DEPS="$DEPS $LIBPATH"
                    SLIBS="$SLIBS $(dirname $LIBPATH)/lib$LIBNAME.a"

                    # load static
                    load-static
                    [ $STATIC -eq 1 ] && load-all-symbols
                    CMD="$CMD -l$LIBNAME"

                fi
            fi

            # not found within our directories
            if [ $FOUND -eq 0 ]
            then

                # do not need to load all symbols for external libs
                [ $STATIC -eq 1 ] && load-ref-symbols

                if [ $STATICSYSLIBS -eq 1 ]
                then
                    case "$LIBNAME" in
                    z|bz2)
                        # set load to static
                        load-static
                        ;;

                    *)
                        # set load to dynamic
                        load-dynamic
                        ;;

                    esac
                else
                    # set load to normal
                    load-dynamic
                fi

                CMD="$CMD -l$LIBNAME"
            fi
            ;;

        -s*)

            # force static load
            FOUND=0
            find-lib $LIBNAME.a $LDIRS
            if [ "$LIBPATH" != "" ]
            then

                # found it
                FOUND=1

                # add it to dependencies
                DEPS="$DEPS $LIBPATH"
                SLIBS="$SLIBS $(dirname $LIBPATH)/lib$LIBNAME.a"

                # load static
                load-static
                [ $STATIC -eq 1 ] && load-all-symbols
                CMD="$CMD -l$LIBNAME"

            fi

            # not found within our directories
            if [ $FOUND -eq 0 ]
            then

                # do not need to load all symbols for external libs
                [ $STATIC -eq 1 ] && load-ref-symbols

                if [ $STATIC -eq 1 ] || [ $STATICSYSLIBS -eq 1 ]
                then

                    # set load to static
                    load-static

                else

                    # special case for libs we have in "ext"
                    # that are sometimes requested as static
                    case "$LIBNAME" in
                    z|bz2)
                        # set load to dynamic
                        load-dynamic
                        ;;
                    *)
                        load-static
                        ;;
                    esac
                fi

                CMD="$CMD -l$LIBNAME"
            fi
            ;;

        -d*)

            FOUND=0
            if [ $STATIC -eq 1 ]
            then
                find-lib $LIBNAME.a $LDIRS
                if [ "$LIBPATH" != "" ]
                then

                    # found it
                    FOUND=1

                    # add it to dependencies
                    DEPS="$DEPS $LIBPATH"
                    SLIBS="$SLIBS $(dirname $LIBPATH)/lib$LIBNAME.a"

                    # load static
                    load-static
                    load-all-symbols

                    CMD="$CMD -l$LIBNAME"

                fi

                # not found within our directories
                if [ $FOUND -eq 0 ]
                then
                    load-static
                    load-all-symbols

                    CMD="$CMD -l$LIBNAME"
                fi
            fi
            ;;


        esac

    done
fi

# return to normal
load-ref-symbols
load-dynamic

# add in pthreads
if [ $THREADS -ne 0 ]
then
    CMD="$CMD -lpthread"
fi

# produce shared library
echo $CMD
$CMD || exit $?

# produce dependencies
if [ "$DEPFILE" != "" ]
then
    echo "$TARG: $DEPS" > "$DEPFILE"
fi
