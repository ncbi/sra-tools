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

#echo $0 $*

# ===========================================================================
# run basic tests on an installation of sra-tools
#   $1 - installation directory
#   $2 - version number
#
# Error codes:
#   1 - bad arguments
#   2 - tests failed

Usage()
{
    echo "Usage: $0 <installation-directory> <version>" >&2
    echo "" >&2
    echo "  installation-directory:  path to SRA toollkit directory, e.g. ./sratoolkit.2.9.0-centos_linux64" >&2
    echo "  version:                 3-part version number of the SRA toolkit, e.g. 2.9.0" >&2
}

RunTool()
{
    #echo RunTool $*
    eval $* >/dev/null 2>&1
    if [ "$?" != "0" ]
    then
        FAILED="${FAILED} $* ;"
    fi
}

if [ "$#" -lt 1 ]
then
    Usage
    exit 0
elif [ "$1" == "-h" ]
then
    Usage
    exit 0
fi

if [ "$1" == "" ]
then
    echo "Missing argument: sratoolkit installation directory"
    Usage
    exit 1
fi
TK_INSTALL_DIR=$1
BIN_DIR=${TK_INSTALL_DIR}bin

if [ "$2" == "" ]
then
    echo "Missing argument: version"
    Usage
    exit 1
fi
VERSION=$2

FAILED=""

################################ TEST sratoolkit ###############################

echo
echo "Smoke testing ${BIN_DIR} ..."

# list all tools; vdb-passwd is obsolete but still in the package
TOOLS=$(ls -1 ${BIN_DIR} | grep -vw ncbi | grep -v vdb-passwd | grep -vE '[0-9]$')

# run all tools with -h and -V

for tool in ${TOOLS}
do

    RunTool ${BIN_DIR}/$tool -h

    # All tools are supposed to respond to -V and --version, yet some respond only to --version, or -version, or nothing at all
    VERSION_OPTION="-V"
    if [ "${tool}" = "blastn_vdb" ]     ; then VERSION_OPTION="-version"; fi
    if [ "${tool}" = "sra-blastn" ]     ; then VERSION_OPTION="-version"; fi
    if [ "${tool}" = "sra-tblastn" ]    ; then VERSION_OPTION="-version"; fi
    if [ "${tool}" = "tblastn_vdb" ]    ; then VERSION_OPTION="-version"; fi
    if [ "${tool}" = "dump-ref-fasta" ] ; then VERSION_OPTION="--version"; fi
    RunTool "${BIN_DIR}/$tool ${VERSION_OPTION} | grep -q ${VERSION}"

done

# run some key tools, check return codes
RunTool ${BIN_DIR}/test-sra
RunTool ${BIN_DIR}/vdb-config
RunTool ${BIN_DIR}/prefetch SRR002749
RunTool ${BIN_DIR}/vdb-dump SRR000001 -R 1
RunTool ${BIN_DIR}/fastq-dump SRR002749 -fasta -Z
RunTool ${BIN_DIR}/sam-dump SRR002749
RunTool ${BIN_DIR}/sra-pileup SRR619505 --quiet

if [ "${FAILED}" != "" ]
then
    echo "Failed: ${FAILED}"
    exit 2
fi

echo "Sratoolkit smoke test successful"

echo

########################### TEST GenomeAnalysisTK.jar ##########################

JAR=GenomeAnalysisTK.jar
echo "Smoke testing ${JAR} ..."

LOG=-Dvdb.log=FINEST
LOG=-Dvdb.log=WARNING
LOG=

GLOG="-l INFO"
GLOG="-l WARN"
#GLOG="-l ERROR"
#GLOG="-l FATAL"
#GLOG="-l OFF"

ARGS=-Dvdb.System.loadLibrary=1

java -version 2>&1 | grep -q 1.7
if [ "$?" = "0" ] ; then # GenomeAnalysisTK was built for java 1.8
    export PATH=/net/pan1.be-md/sra-test/bin/jre1.8.0_171/bin:$PATH
fi

CL=org.broadinstitute.gatk.engine.CommandLineGATK
L="-L NC_000020.10:61000001-61001000"

# execute when dll download is disabled and dll-s cannot be located: should fail

ACC=SRR835775
GARG="-T UnifiedGenotyper -I ${ACC} -R ${ACC} ${L} -o S.vcf"
cmd="java ${LOG} ${ARGS} -cp ./${JAR} ${CL} ${GARG} ${GLOG}"

echo
echo ${cmd}
eval ${cmd} 2>/dev/null
if [ "$?" = "0" ] ; then
    FAILED="${FAILED} ${JAR} with disabled smart dll search;"
fi

# execute when dll download is enabled

PWD=`pwd`
ARGS=-Duser.home=${PWD}

GARG="-T UnifiedGenotyper -I ${ACC} -R ${ACC} ${L} -o S.vcf"
cmd="java ${LOG} ${ARGS} -cp ./${JAR} ${CL} ${GARG} ${GLOG}"

echo
echo ${cmd}
eval ${cmd} >/dev/null
if [ "$?" != "0" ] ; then
    FAILED="${FAILED} ${JAR};"
fi

# execute with "-jar GenomeAnalysisTK.jar"

GARG="-T HaplotypeCaller -R ${ACC} -I ${ACC} -o SRR8179.vcf ${L}"
cmd="java ${LOG} ${ARGS} -jar ./${JAR} ${GARG} ${GLOG}"
echo
echo ${cmd}
eval ${cmd} >/dev/null
if [ "$?" != "0" ] ; then
    FAILED="${FAILED} -jar ${JAR};"
fi

echo

if [ "${FAILED}" != "" ]
then
    echo "Failed: ${FAILED}"
    exit 3
fi

echo "${JAR} smoke test successful"

########################## TEST broken symbolic links ##########################
echo
echo "Checking broken symbolic links ..."
echo    find . -type l ! -exec test -e {} \; -print
FAILED=`find . -type l ! -exec test -e {} \; -print`
if [ "${FAILED}" != "" ]
then
    echo "Failed: found broken symbolic links ${FAILED}" | tr '\n' ' '
    echo
    exit 4
fi
