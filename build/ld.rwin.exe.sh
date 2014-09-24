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
if [ "$VERBOSE" != "" ] ; then echo "$0 $*"; fi

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

# parameters and common functions
source "${0%rwin.exe.sh}win.cmn.sh"

# discover tool chain
case "$LD" in
link)
    source "${0%exe.sh}vc++.sh"
    ;;
*)
    echo "$SELF_NAME: unrecognized ld tool - '$LD'"
    exit 5
esac

#echo "EXE_CMD=${EXE_CMD}"

# EXE_CMD was started in tool-specific source
CMD="$EXE_CMD $LDFLAGS OLE32.lib Ws2_32.lib Shell32.lib"

# if building a static executable against dynamic libraries
# the main application will substitute for name lookup
if [ $STATIC -eq 1 ] && [ $DYLD -eq 1 ]
then
    CMD="$CMD $LD_EXPORT_GLOBAL $LD_MULTIPLE_DEFS"
fi

# function to convert static libraries to individual COFF files
convert-static ()
{
    # list members
    local path="$1"
    local mbrs="$(ar -t $path)"

    # create sub directory
    rm -rf "$2" && mkdir "$2"
    if ! cd "$2"
    then
        echo "$SELF_NAME: failed to cd to $2"
        exit 5
    fi
    ar -x "$path"

    # add source files to link
    local m=
    for m in $mbrs
    do
        CMD="$CMD $2/$m"
    done

    # return to prior location
    cd - > /dev/null
}

# tack on object files
CMD="$CMD $OBJS"

# initial dependency upon Makefile - no vers file on Windows
DEPS="$SRCDIR/Makefile"
if [ "$LIBS" != "" ]
then
    # tack on paths
    DIRS="$LDIRS:$XDIRS"

    while [ "$DIRS" != "" ]
    do
        DIR="${DIRS%%:*}"
        CURDIR="$RHOME/${DIR#$LHOME}"
        CURDIR="$(echo $CURDIR | tr '/' '\\')"        
        [ "$CURDIR" != "" ] && CMD="$CMD /LIBPATH:$CURDIR"        
        DIRS="${DIRS#$DIR}"
        DIRS="${DIRS#:}"
    done
    
    HAVE_KERNEL32=0
    HAVE_WS2=1
    HAVE_CLIB=0

    # tack on libraries, finding as we go
    for xLIB in $LIBS
    do

        # strip off switch
        xLIBNAME="${xLIB#-[lsd]}"

        # map xLIBNAME
        case "$xLIBNAME" in
        dl)
            if [ $HAVE_KERNEL32 -ne 1 ]
            then
                load-ref-symbols
                load-dynamic
                CMD="$CMD Kernel32.lib"
                HAVE_KERNEL32=1
            fi
            continue
            ;;

        ws2)
            if [ $HAVE_WS2 -ne 1 ]
            then
                load-ref-symbols
                load-dynamic
                CMD="$CMD ws2_32.lib"
                HAVE_WS2=1
            fi
            continue
            ;;

        # redirect libm to link against libc.lib in case of windows
        # omitting the lib defaults to linking against libc.lib
        m)
            if [ $HAVE_CLIB -ne 1 ]
            then
                load-ref-symbols
                load-dynamic
                HAVE_CLIB=1
            fi
            continue
            ;;

##### TEMPORARY #####
# use ksproc for kproc
#    kproc)
#        xLIBNAME=ksproc
#        ;;
#####################
        esac

        # look at linkage
        case "$xLIB" in
        -l*)

            # normal or dynamic linkage
            FOUND=0
            if [ $STATIC -eq 0 ]
            then
                find-lib $xLIBNAME.lib $LDIRS
                if [ "$xLIBPATH" != "" ]
                then

                    # found it
                    FOUND=1

                    # load dynamic
                    load-dynamic
                    CMD="$CMD lib$xLIBNAME.lib"

                fi
            fi

            # try static only
            if [ $FOUND -eq 0 ]
            then
                find-lib $xLIBNAME.a $LDIRS
                if [ "$xLIBPATH" != "" ]
                then

                    # found it
                    FOUND=1

                    # add it to dependencies
                    DEPS="$DEPS $xLIBPATH"

                    # load static
                    load-static
                    [ $STATIC -eq 1 ] && load-all-symbols
                    convert-static "$xLIBPATH" "lib$xLIBNAME"

                fi
            fi

            # not found within our directories
            if [ $FOUND -eq 0 ]
            then
                [ $STATIC -eq 1 ] && load-ref-symbols
                load-dynamic
                CMD="$CMD lib$xLIBNAME.lib"
            fi
            ;;

        -s*)

            # force static load
            FOUND=0
            find-lib $xLIBNAME.a $LDIRS
            if [ "$xLIBPATH" != "" ]
            then

                # found it
                FOUND=1

                # add it to dependencies
                DEPS="$DEPS $xLIBPATH"

                # load static
                load-static
                [ $STATIC -eq 1 ] && load-all-symbols
                convert-static "$xLIBPATH" "lib$xLIBNAME"

            fi

            # not found within our directories
            if [ $FOUND -eq 0 ]
            then
                # set load to static
                load-static
                [ $STATIC -eq 1 ] && load-all-symbols
                CMD="$CMD lib$xLIBNAME.lib"
            fi
            ;;

        -d*)

            FOUND=0
            if [ $STATIC -eq 1 ]
            then
                find-lib $xLIBNAME.a $LDIRS
                if [ "$xLIBPATH" != "" ]
                then

                    # found it
                    FOUND=1

                    # add it to dependencies
                    DEPS="$DEPS $xLIBPATH"

                    # load static
                    load-static
                    load-all-symbols
                    convert-static "$xLIBPATH" "lib$xLIBNAME"

                fi

                # not found within our directories
                if [ $FOUND -eq 0 ]
                then
                    load-static
                    load-all-symbols

                    CMD="$CMD lib$xLIBNAME"
                fi
            fi
            ;;


        esac

    done
fi

# return to normal
load-ref-symbols
load-dynamic

# determine current directory
CURDIR="$(pwd)"

# produce executable
rm -f ${TARG%exe}*

echo $CMD

# Windows linker crashes randomly on bigger files with rc=1000, so we will loop until it completes differently
while [ 1 ]
do
    ${TOP}/build/run_remotely.sh $PROXY_TOOL $RHOST $RPORT $RHOME $LHOME $CURDIR $ROUTDIR $LOUTDIR $CMD >${TARG}.out
    STATUS=$?
    cat ${TARG}.out
    if [ "$STATUS" != "0" ]
    then 
        grep "fatal error LNK1000" ${TARG}.out >/dev/null
        if [ "$?" != "0" ] 
        then 
            exit $STATUS
        fi
    else
        rm ${TARG}.out
        break
    fi
    sleep 30s
done    

# wait for the result file to appear (there may be a network delay)
${TOP}/build/wait_for_file.sh ${TARG}.exe
STATUS=$?
if [ "$STATUS" = "1" ]
then
    echo "timed out, TARG='$TARG'"
    exit $STATUS
fi

# create a link without .exe which represents make's target
test -e ${TARG} || ln -s ${TARG}.exe ${TARG}

# produce dependencies
if [ "$DEPFILE" != "" ]
then
    echo "$TARG: $DEPS" > "$DEPFILE"
fi

# cleanup temporary files
rm -f $TARG.lib $TARG.exp


# wait for the result file to appear (there may be a network delay)
${TOP}/build/wait_for_file.sh ${TARG}.exe
STATUS=$?
if [ "$STATUS" = "1" ]
then
    echo "timed out, TARG='$TARG'"
    exit $STATUS
fi

# create a link without .exe which represents make's target
test -e ${TARG} || ln -s ${TARG}.exe ${TARG}

# produce dependencies
if [ "$DEPFILE" != "" ]
then
    echo "$TARG: $DEPS" > "$DEPFILE"
fi

# cleanup temporary files
rm -f $TARG.lib $TARG.exp
