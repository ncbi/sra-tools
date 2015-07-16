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

#
#   Download and test SRA Toolkit tarballs
#
# $1 - work directory
#
# return codes:
# 0 - passed
# 1 - wget failed
# 2 - gunzip failed
# 3 - tar failed
# 4 - one of the tools failed

WORKDIR=$1
if [ "$WORKDIR" == "" ]
then
    WORKDIR="./temp"
fi

TOOLS="abi-dump abi-load align-info bam-load blastn_vdb cache-mgr cg-load fastq-dump fastq-load helicos-load illumina-dump \
illumina-load kar kdbmeta latf-load prefetch rcexplain sam-dump sff-dump sff-load sra-kar sra-pileup \
sra-sort sra-stat srapath srf-load tblastn_vdb test-sra vdb-config vdb-copy vdb-decrypt vdb-dump vdb-encrypt vdb-lock \
vdb-unlock vdb-validate"

# vdb-passwd is obsolete but still in the package

case $(uname) in
Linux)
    python -mplatform | grep Ubuntu && OS=ubuntu64 || OS=centos_linux64
    TOOLS="$TOOLS pacbio-load remote-fuser"
    ;;
Darwin)
    OS=mac64
    ;;
esac

TARGET=sratoolkit.current-$OS

mkdir -p $WORKDIR
cd $WORKDIR
wget http://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/$TARGET.tar.gz || exit 1
gunzip -f $TARGET.tar.gz || exit 2
PACKAGE=$(tar tf $TARGET.tar | head -n 1)
rm -rf $PACKAGE
tar xvf $TARGET.tar || exit 3

FAILED=""
for tool in $TOOLS 
do
    echo $tool
    $PACKAGE/bin/$tool -h >/dev/null 
    if [ "$?" != "0" ]
    then
        echo "$(pwd)/$PACKAGE/bin/$tool failed" 
        FAILED="$FAILED $tool" 
    fi
done

if [ "$FAILED" != "" ]
then
    echo "The following tools failed: $FAILED"
    exit 4
fi

cd -
rm -rf $WORKDIR



