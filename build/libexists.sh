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

# libexists.sh: test whether one of the given libraries defines all of the specified external names
# Usage:
#   bash libexists.sh OS ARCH CC LD CC_SH LD_SH [-L dir ] [-X dir] [-o dir] [-l lib] [-s lib] [-d lib] [-v] extName1 [ extName2 ... ]
# Parameters:
#   OS : linux/mac/win
#   ARCH : i386/x86_64
#   CC : compiler command
#   LD : link command
#   CC_SH : compile script
#   LD_SH : link script
#   extName1, ... - external names to find in libraries
# Options:
#   -L, -X :    library directories
#   -o :        directory for intermediate files
#   -l, -d :    libraries to search (shared or static)
#   -s :        libraries to search (static)
#   -v :        verbose mode
# Environment:
#   requires an ?? OS.ARCH.sh file in the script's directory
# Return: 0 if found, the library's name is on stdout
#         1 not found
#        -1 bad arguments
#

BUILD_DIR="$(dirname $0)"

# os
OS="$1";shift
# architecture
ARCH="$1";shift
# compiler
CC="$1";shift 1
# linker
LD="$1";shift
# compile script
CC_SH="$1";shift
# link script
LD_SH="$1";shift

# message prefix for verbose mode
dbgPref="**$(basename $0): "

# parse parameters
cOpts=
libs=
verbose="false"
outDir="."
while getopts L:X:l:s:d:vo: o
do	case "$o" in
	L|X)	cOpts="${cOpts} -L$OPTARG";libPath="${libPath}:${OPTARG}";;
	l|d)	libs="${libs} l$OPTARG";; 
	s)	    libs="${libs} s$OPTARG";; 
	o)      outDir="$OPTARG";;
	v)	    verbose="true";; 
	[?])	printf >&2 "Usage: $0 [-L dir ] [-X dir] [-o dir] [-l lib] [-s lib] [-d lib] [-v] extName1 [ extName2 ... ]"
		exit -1;;
	esac
done
libPath="${outDir}${libPath}"

shift $((OPTIND - 1))

if [ $verbose == "true" ]
then
    printf >&2 "${dbgPref}OS   ='%s'\n"  "$OS"
    printf >&2 "${dbgPref}ARCH   ='%s'\n"  "$ARCH"
    printf >&2 "${dbgPref}CC   ='%s'\n"  "$CC"
    printf >&2 "${dbgPref}LD   ='%s'\n"  "$LD"
    printf >&2 "${dbgPref}CC_SH   ='%s'\n"  "$CC_SH"
    printf >&2 "${dbgPref}LD_SH   ='%s'\n"  "$LD_SH"
    printf >&2 "${dbgPref}cOpts   ='%s'\n"  "$cOpts"
    printf >&2 "${dbgPref}libs    ='%s'\n"  "$libs"
    printf >&2 "${dbgPref}verbose ='%s'\n"  $verbose
    printf >&2 "${dbgPref}outDir  ='%s'\n"  $outDir
fi

## create a C program referencing all the external names
srcFile="${outDir}/libexists.c"
objFile="${outDir}/libexists.o"
exeFile="${outDir}/libexists"

echo "" >${srcFile}
for fn in $* 
do  #declare
    echo "extern void ${fn}(void);" >>${srcFile}
done
echo "int main( int argc, char *argv [] ){" >>${srcFile}
echo "if (argc < 0 ) {    " >>${srcFile}
for fn in $* 
do
    echo "   ${fn}();" >>${srcFile}
done
echo "} return 0;} int wmain(int argc, char *argv []) { return main(argc,argv); }" >>${srcFile}

if [ $verbose == "true" ]
then
    echo "${dbgPref}created ${srcFile}:" >&2
    cat ${srcFile} >&2
fi
##

## compile
compile="${BUILD_DIR}/$CC_SH $OS $CC -c -o ${objFile} ${srcFile}"
if [ $verbose == "true" ]
then
    echo "${dbgPref}${compile}" >&2
fi
compileRes=$($compile)
if [ $verbose == "true" ]
then
    echo "${dbgPref}${compileRes}" >&2
fi
##

#try to link with all specified libraries until successful
LD_LIBRARY_PATH=${libPath}:$LD_LIBRARY_PATH; export LD_LIBRARY_PATH
rc=1
for lib in $libs
do
    pref=${lib:0:1}
    lib=${lib:1}
    if [ $pref == "s" ]
    then
        link="${BUILD_DIR}/$LD_SH $OS $ARCH $LD --static --exe ${cOpts} -l$lib -o${exeFile} --objx o ${objFile}"
    else
        link="${BUILD_DIR}/$LD_SH $OS $ARCH $LD          --exe ${cOpts} -l$lib -o${exeFile} --objx o ${objFile}" 
    fi    
    if [ $verbose == "true" ]
    then
        echo ${dbgPref} "$link" >&2
    fi
    linkRes=$($link 2>/dev/null)
    linkRc=$?
    if [ $verbose == "true" ]
    then
        echo ${dbgPref} "$linkRes" >&2
    fi
    
    if [ $linkRc == 0 ]
    then 
        ${exeFile} 2>/dev/null
        if [ $? == 0 ]
        then 
            # Success: RC=0, found library's name to stdout
            rc=0
            echo $lib
            break
        fi            
    fi
done

#clean up
rm -f ${srcFile} ${objFile} ${exeFile}

exit $rc 
