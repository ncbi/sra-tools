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

BUILD_DIR="$(dirname $0)"
TOP="$(dirname $BUILD_DIR)"
SCM_DIR="$TOP/scm"

MODULE_NAME="$1"
CURMD5="$2"
VERS_FILE="$3"

# if the given versfile does not exist, bail out with no error
[ -f "$VERS_FILE" ] || exit 0

echo "*** start of scm-handler for $MODULE_NAME ***************************"

#prepare the filenames we will use for the version-increment
LOGFILE="$SCM_DIR/scm.log"
PREVMD5="$SCM_DIR/$MODULE_NAME.pub"
CANDMD5="$SCM_DIR/$MODULE_NAME.cand"

# write a log entry, that a scm-run for this module is executed
echo -e "\n$(date) ************* scm-run for $MODULE_NAME" >> $LOGFILE

# if the current md5-file does not exist, bail out with error
if [ ! -f "$CURMD5" ]
then
    MSG="current md5-file ($CURMD5) does not exist! SCM RUN TERMINATED!"
    echo "$MSG"
    echo "$MSG" >> $LOGFILE
    exit 6
fi


# check if the current md5-file has the same content as in the
# candidate md5-file, if yes bail out without error
if diff -q "$CURMD5" "$CANDMD5" > /dev/null 2>&1
then
    MSG="cand. and curr. md5-files are equal, nothing to do!"
echo "$MSG"
    echo "$MSG" >> $LOGFILE
    exit 0
fi

# check if the candidate md5-file does exist
if [ ! -f "$CANDMD5" ]
then
    MSG="candidate md5-file does not exist."
    echo "$MSG"
    echo "$MSG" >> $LOGFILE

    # if candidate-md5-file does not exist
    # the last published too should not exist
    if ! cp "$CURMD5" "$CANDMD5"
    then
        STATUS=$?
        MSG="unable to create the candidate-md5-file"
        echo "$MSG"
        echo "$MSG" >> $LOGFILE
        exit $STATUS
    fi

    # turn this on later
    cvs add "$CANDMD5"
    exit 0
fi


# if we reach this line, cand. and curr md5-file do exist
# and differ in content
MSG="cand. and curr. md5-files are different."
echo "$MSG"
echo "$MSG" >> $LOGFILE

# check if the current md5-file has the same content as in the
# previously published md5-file, if yes bail out without error
if diff -q "$CANDMD5" "$PREVMD5" > /dev/null 2>&1
then
    CAND_PREV_EQUAL=1
else
    CAND_PREV_EQUAL=0
fi

# if the candidate-md5-file does exist, 
# copy the current md5-file into the candidate-md5-file
if ! cp "$CURMD5" "$CANDMD5"
then
    STATUS=$?
    MSG="unable to update the candidate-md5-file"
    echo "$MSG"
    echo "$MSG" >> $LOGFILE
    exit $STATUS
fi

if [ $CAND_PREV_EQUAL -eq 0 ]
then
    MSG="candidate and prev. published md5-files differ, nothing to do!"
echo "$MSG"
    echo "$MSG" >> $LOGFILE
    exit
fi

# if a candidate file does not exist, the version has to be incremented
OLDVERS=$(cat "$VERS_FILE")
NEWVERS=$($BUILD_DIR/increment-release.sh $OLDVERS)
MSG="version incremented from $OLDVERS to $NEWVERS"
echo "$MSG"
echo "$MSG" >> $LOGFILE
echo $NEWVERS > $VERS_FILE

# trigger a rebuild ( of this module )...
touch "$BUILD_DIR/version-rebuild"

echo "*** end of scm-handler for $MODULE_NAME *****************************"
