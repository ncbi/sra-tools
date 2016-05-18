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


ARCHIVE="newkar.kar"
INPUT="testsource"
OUTPUT="output"
EXPECTED="expected/old-kar"


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
    else
        # compare against legacy output
        RESULT="stat.txt"
        stat $ARCHIVE | grep -w "Size" > $RESULT
        if ! diff -q -w $EXPECTED/stat.txt $RESULT > /dev/null
        then
            echo "KAR file differs from legacy...test failed"
            cleanup
            exit
        fi
        rm $RESULT
    fi

    echo "   Testing create mode with existing archive..."
    if $KAR -c $ARCHIVE -d $INPUT 2> /dev/null
    then
        STATUS=$?
        echo "KAR create operation with existing archive failed to produce an error"
        cleanup
        exit
    fi

    echo "   Testing force create mode..."
    if ! $KAR -f -c $ARCHIVE -d $INPUT
    then
        STATUS=$?
        echo "KAR force create operation failed"
        cleanup
        exit
    fi

    echo "   Testing option variation --create ..."
    if ! $KAR -f --create $ARCHIVE -d $INPUT
    then
        STATUS=$?
        echo "KAR create operation failed"
        cleanup
        exit
    fi

    echo "   Testing option variation --directory ..."
    if ! $KAR -f --create $ARCHIVE --directory $INPUT
    then
        STATUS=$?
        echo "KAR create operation failed"
        cleanup
        exit
    fi

    echo "   Testing option variation --force ..."
    if ! $KAR --force --create $ARCHIVE --directory $INPUT
    then
        STATUS=$?
        echo "KAR create operation failed"
        cleanup
        exit
    fi
    echo "Passed"

}

test_test ()
{
    echo "Testing test mode..."

    RESULT="newkar.txt"
    $KAR -t $ARCHIVE > $RESULT
    if ! diff -q -w $EXPECTED/list.txt $RESULT > /dev/null
    then
        echo "KAR listing differs from legacy...test failed"
        cat $RESULT
        cleanup
        exit
    fi
    rm $RESULT

    echo "   Testing option variation --test ..."
    $KAR --test $ARCHIVE > $RESULT
    if ! diff -q -w $EXPECTED/list.txt $RESULT > /dev/null
    then
        echo "KAR listing differs from legacy...test failed"
        cat $RESULT
        cleanup
        exit
    fi
    rm $RESULT

    echo "   Testing legacy longlist mode..."
    $KAR -l -t $ARCHIVE | tr -s [' '] | cut -d' '  -f 2,4,5 > $RESULT
    if ! diff -q -w $EXPECTED/longlist.txt $RESULT > /dev/null
    then
        echo "KAR listing differs from legacy...test failed"
        cat $RESULT
        cleanup
        exit
    fi
    rm $RESULT

    echo "Passed"
}

test_extract ()
{
    echo "Testing extract mode..."

    RESULT="extracted"
    $KAR -x $ARCHIVE -d $RESULT
    if ! diff -q -r $EXPECTED/extracted $RESULT > /dev/null
    then
        echo "KAR extracting content differs from legacy...test failed"
        cleanup
        exit
    fi
    chmod -R +w $RESULT
    rm -rf $RESULT

    echo "   Testing option variation --extract ..."
    $KAR --extract $ARCHIVE -d $RESULT
    if ! diff -q -r $EXPECTED/extracted $RESULT > /dev/null
    then
        echo "KAR extracting content differs from legacy...test failed"
        cleanup
        exit
    fi

    chmod -R +w $RESULT
    rm -rf $RESULT
    cleanup

    echo "Passed"
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
