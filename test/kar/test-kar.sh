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


TESTDIR="./kar_testsource"
ARCHIVE="kar.kar"

KAR=kar
[ $# -ge 1 ] && KAR="$1"

testdir_setup ()
{
    if ! mkdir $TESTDIR
    then
        echo "Failed to create testsource"
        exit 1
    fi

    echo "random text for test purposes" > $TESTDIR/test_file1
    echo "random text for test purposes and more" > $TESTDIR/test_file2
}

cleanup ()
{
    rm -rf $TESTDIR $ARCHIVE $ARCHIVE.md5
}


# if test source directory doesnt exist, make it and populate it
if [ ! -d $TESTDIR ]
then
    testdir_setup
fi

# run the script
if ! $KAR --md5 -f -d $TESTDIR -c $ARCHIVE
then
    STATUS=$?
    echo "KAR md5 operation failed"
else
    case $(uname) in
        (Linux)
            if ! md5sum -c $ARCHIVE.md5
            then
                STATUS=$?
                echo "md5sum check failed with status $STATUS"
            fi
            ;;
        (Darwin)
            if ! MD5=$(md5 $ARCHIVE)
            then
                STATUS=$?
                echo "md5 failed with status $STATUS"
            else
                MY_MD5=$(cut -f1 -d' ' $ARCHIVE.md5)
                if [ "$MD5" == "$MY_MD5" ]
                then
                    echo "$ARCHIVE: OK"
                else
                    echo "$ARCHIVE: FAILED"
                    STATUS=1
                fi
            fi
            ;;
        (*)
            STATUS=1
            echo unknown platform
            ;;
    esac
fi


exit $STATUS
