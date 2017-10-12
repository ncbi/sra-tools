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

# install-kfg.sh
#   copies file $1 from $2 to $3.
#   Will create a backup copy if the file's md5 is not found in $4 (assumed to be a user's edit)
#

FILE_NAME=$1
SRC_DIR=$2
KONFIG_DIR=$3
MD5SUMS=$4

SRC_FILE=$2/$1
TGT_FILE=$3/$1

mkdir -p ${KONFIG_DIR}

#echo "installing $1 from $2 to $3, mdsums = $4"

# create a backup if installed file has been modified by user
if [ -f ${TGT_FILE} ] ; then
    md5=$(md5sum ${TGT_FILE} | awk '{print $1;}')
    #echo "$1 md5=$md5"
    if [ "$(grep ${md5} ${MD5SUMS})" == "" ] ; then
        # not a known version of the file; create a backup copy
        mv -b -v ${TGT_FILE} ${TGT_FILE}.orig
    fi
fi

# copy to the install location
cp ${SRC_FILE} ${TGT_FILE}
