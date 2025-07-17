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

export NCBI_VDB_NO_ETC_NCBI_KFG=1
export NCBI_SETTINGS=/

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
    echo RunTool $*
    $* >/dev/null 2>&1
    RC="$?"
    if [ "$RC" != "0" ]
    then
        echo Returned $RC
        FAILED="${FAILED} $* ;"
    #else
    #    echo Returned $RC
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

if [ -d "${TK_INSTALL_DIR}/bin" ]
then
    BIN_DIR=${TK_INSTALL_DIR}/bin
    KFG=${BIN_DIR}/ncbi/tmp.kfg
else
    BIN_DIR=${TK_INSTALL_DIR}/usr/local/ncbi/sra-tools/bin
    KFG=${TK_INSTALL_DIR}/etc/ncbi
    export VDB_CONFIG=${KFG}
    KFG=${KFG}/tmp.kfg
fi

if [ "$2" == "" ]
then
    echo "Missing argument: version"
    Usage
    exit 1
fi
VERSION=$2

FAILED=""

################################ TEST sratoolkit ###############################

echo "/LIBS/GUID = \"8badf00d-1111-4444-8888-deaddeadbeef\"" > ${KFG}

echo
echo "Smoke testing ${BIN_DIR} ..."

# list all tools; samview-util is ignored; sratools ios not to be run on its own
TOOLS=$(ls -1 ${BIN_DIR} | grep -vw ncbi | grep -v samview-util | grep -v sratools | grep -vE '[0-9]$')

# run all tools with -h and -V

for tool in ${TOOLS}
do

    # All tools are supposed to respond to -V and --version, yet some respond only to --version, or -version, or nothing at all
    VERSION_OPTION="-V"
    if [ "${tool}" = "blastn_vdb" ]     ; then VERSION_OPTION="-version"; fi
    if [ "${tool}" = "sra-blastn" ]     ; then VERSION_OPTION="-version"; fi
    if [ "${tool}" = "sra-tblastn" ]    ; then VERSION_OPTION="-version"; fi
    if [ "${tool}" = "tblastn_vdb" ]    ; then VERSION_OPTION="-version"; fi
    if [ "${tool}" = "dump-ref-fasta" ] ; then VERSION_OPTION="--version"; fi
    
    RunTool ${BIN_DIR}/$tool -h
    RunTool "${BIN_DIR}/$tool ${VERSION_OPTION} | grep ${VERSION}"

done

# run some key tools, check return codes
RunTool ${BIN_DIR}/test-sra
RunTool ${BIN_DIR}/vdb-config
RunTool ${BIN_DIR}/prefetch SRR002749
RunTool ${BIN_DIR}/vdb-dump SRR000001 -R 1
RunTool ${BIN_DIR}/fastq-dump SRR002749 --fasta 0 -Z
RunTool ${BIN_DIR}/sam-dump SRR002749
RunTool ${BIN_DIR}/sra-pileup SRR619505 # --quiet

if [ "${FAILED}" != "" ]
then
    echo "Failed: ${FAILED}"
    exit 2
fi

echo "Sratoolkit smoke test successful"

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

# run an example
EXAMPLE="${BIN_DIR}/vdb-dump SRR000001 -R 1"
$EXAMPLE | grep -q EM7LVYS02FOYNU
if [ "$?" != "0" ]
then
    echo "The example failed: $EXAMPLE"
    exit 9
fi

# test a run with a vdbcache
EXAMPLE="${BIN_DIR}/vdb-dump --table_enum SRR390728"
$EXAMPLE | grep -q SEQUENCE
if [ "$?" != "0" ]
then
    echo "The example failed: $EXAMPLE"
    exit 9
fi

rm -f ${KFG}
