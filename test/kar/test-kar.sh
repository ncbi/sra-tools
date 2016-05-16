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


ARCHIVE="kar.kar"
INPUT="kar_testsource"
OUTPUT="output"

KAR=kar
[ $# -ge 1 ] && KAR="$1"


cleanup ()
{
    rm -rf $ARCHIVE $ARCHIVE.md5
}

test_create ()
{
    echo "Testing create mode..."
    if ! $KAR -c $ARCHIVE -d $INPUT
    then
        STATUS=$?
        echo "KAR create operation failed"
        cleanup
        exit
    fi
    echo "Passed"

    echo "Testing create mode with existing archive..."
    if $KAR -c $ARCHIVE -d $INPUT
    then
        STATUS=$?
        echo "KAR create operation with existing archive failed fail"
        cleanup
        exit
    fi
    echo "Passed"

    echo "Testing force create mode..."
    if ! $KAR -f -c $ARCHIVE -d $INPUT
    then
        STATUS=$?
        echo "KAR force create operation failed"
        cleanup
        exit
    fi
    echo "Passed"    

    cleanup
}

test_test ()
{
    echo "Testing test mode..."
    if ! $KAR -c $ARCHIVE -d $INPUT
    then
        STATUS=$?
        echo "KAR test operation failed"
    else
        $KAR -t $ARCHIVE
    fi
    echo "Passed"

    cleanup
}

test_extract ()
{
    echo "Testing extract mode..."

    if ! $KAR -c $ARCHIVE -d $INPUT
    then
        STATUS=$?
        echo "KAR create operation failed"
    else
        $KAR -x $ARCHIVE -d $OUTPUT
        tree output/
    fi

    echo "Passed"

    cleanup
    chmod -R +w $OUTPUT
    rm -rf $OUTPUT
}

test_md5 ()
{
    echo "Testing create mode with md5..."
    if ! $KAR --md5 -f -c $ARCHIVE -d $INPUT
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

    echo "Passed"

    cleanup
}

# run the script
test_create
test_test
test_extract
test_md5



exit $STATUS
