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

usage()
{
    echo "Usage: $0 <installation-directory> <version>" >&2
    echo "" >&2
    echo "  installation-directory:  path to SRA toollkit directory, e.g. ./sratoolkit.2.9.0-centos_linux64" >&2
    echo "  version:                 3-part version number of the SRA toolkit, e.g. 2.9.0" >&2
}

if [ "$#" -lt 1 ]
then
    usage
    exit 0
elif [ "$1" == "-h" ]
then
    usage
    exit 0
fi

if [ "$1" == "" ]
then
    echo "Missing argument: installation directory"
    usage
    exit 1
fi
INSTALL_DIR=$1
BIN_DIR=${INSTALL_DIR}/bin

if [ "$2" == "" ]
then
    echo "Missing argument: version"
    usage
    exit 1
fi
VERSION=$2

#list all tools vdb-passwd is deprecated but still distributed
TOOLS=$(ls -1 ${BIN_DIR} | grep -vw ncbi | grep -v vdb-passwd | grep -vE '[0-9]$')
#echo TOOLS=$TOOLS

# some tools do not respond well to --version and/or -V
# should be fixed, but let them pass for now
NO_VERSION=$(ls -1 ${BIN_DIR} | grep -vw ncbi | grep -v vdb-passwd | grep -vE '[0-9]$' | grep -v blastn_vdb  | grep -v dump-ref-fasta  | grep -v sra-blastn  | grep -v sra-search  | grep -v sra-tblastn  | grep -v tblastn_vdb)
#echo NO_VERSION=$NO_VERSION

echo "Smoke testing ${BIN_DIR} ..."

# run all tools with -h and -V
FAILED=""

for tool in ${TOOLS}
do
    #echo $tool -h
    ${BIN_DIR}/$tool -h >/dev/null
    if [ "$?" != "0" ]
    then
        echo "${BIN_DIR}/$tool -h failed"
        FAILED="${FAILED} $tool"
    fi
done

for tool in ${NO_VERSION}
do
    #echo $tool -V
    ${BIN_DIR}/$tool --version | grep -q ${VERSION}
    if [ "$?" != "0" ]
    then
        echo "${BIN_DIR}/$tool --version failed"
        FAILED="${FAILED} $tool"
    fi
done

# a quick connectivity test

${BIN_DIR}/vdb-dump SRR000001 -R1 | grep -q EM7LVYS02FOYNU
if [ "$?" != "0" ]
then
    echo "${BIN_DIR}/vdb-dump SRR000001 -R1 failed"
    FAILED="${FAILED} $vdb-dump"
fi

if [ "${FAILED}" != "" ]
then
    echo "These tools failed: ${FAILED}"
    exit 2
fi

echo "Smoke test successful"

