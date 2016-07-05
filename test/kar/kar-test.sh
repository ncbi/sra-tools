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


RESULT="result"
ARCHIVE="newkar.kar"
INPUT="source"
OUTPUT="output"


KAR=kar
[ $# -ge 1 ] && KAR="$1"

OLDKAR=./old-kar

cleanup ()
{
    rm -rf $ARCHIVE $ARCHIVE.md5 $RESULT/* $RESULT

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

    echo "   Testing create mode with existing archive..."
    if $KAR -c $ARCHIVE -d $INPUT 2> /dev/null
    then
        STATUS=$?
        echo "KAR create operation with existing archive failed to produce an error"
        cleanup
        exit
    fi
}

test_create_options ()
{
    echo "   Testing force: -f create mode..."
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

    echo "   Testing md5: --md5 mode..."
    if ! $KAR --md5 -f -c $ARCHIVE -d $INPUT
    then
        STATUS=$?
        echo "KAR md5 operation failed"
    else
        case $(uname) in
            (Linux)
                if ! md5sum -c $ARCHIVE.md5 > /dev/null
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
}

test_list ()
{
    echo "Testing test mode..."    
    if ! $KAR -t $ARCHIVE > /dev/null
    then
        echo "KAR listing failed"
        cleanup
        exit
    fi

    echo "   Testing option variation --test ..."
    if ! $KAR --test $ARCHIVE > /dev/null
    then
        echo "KAR listing failed"
        cleanup
        exit
    fi
}

test_list_options ()
{
    echo "   Testing longlist: -l mode..."
    if ! $KAR -l -t $ARCHIVE > /dev/null
    then
        echo "KAR long listing failed"
        cleanup
        exit
    fi
}

test_extract ()
{
    echo "Testing extract mode..."   
    DIR="extracted"
    if ! $KAR -x $ARCHIVE -d $DIR
    then
        echo "KAR extraction failed"
        cleanup
        exit
    fi

    rm -rf $DIR

    echo "   Testing extract mode --extract..."   
    DIR="extracted"
    if ! $KAR --extract $ARCHIVE -d $DIR
    then
        echo "KAR extraction failed"
        cleanup
        exit
    fi

    rm -rf $DIR
}


# create archive with new tool
# test and extract with legacy
test_cNew_txOld_cmp ()
{
    echo "Testing new archive - list and extract with old..."

    mkdir $RESULT

    $KAR -f -c $ARCHIVE -d $INPUT
    
    if ! $OLDKAR -t $ARCHIVE > $RESULT/olist.txt
    then
        echo "      legacy-KAR could not test the archive...failed"
        cleanup
        exit
    fi

    if ! $OLDKAR -l -t $ARCHIVE > $RESULT/ollist.txt
    then
        echo "      legacy-KAR could not test the archive with longlist...failed"
        cleanup
        exit
    fi

    if ! $OLDKAR -x $ARCHIVE -d $RESULT/o_extracted
    then
        echo "      legacy-KAR could not extract the archive...failed"
        cleanup
        exit
    fi


    #start comparing outputs
    echo "   Comparing list outputs..."
    $KAR -t $ARCHIVE > $RESULT/nlist.txt
    if ! diff -w $RESULT/olist.txt $RESULT/nlist.txt > $RESULT/diff.txt
    then
        echo "      KAR listing differs from legacy...test failed"
        cat $RESULT/diff.txt
        cleanup
        exit
    fi

    # there is no purpose in testing long list mode. 
    # most of the differences are an attempt to correct errors in the old tool

    echo "   Comparing extracted archives..."
    $KAR -x $ARCHIVE -d $RESULT/n_extracted
    if ! diff -q -r $RESULT/o_extracted $RESULT/n_extracted 2> $RESULT/diff.txt
    then
        grep -v "alias" $RESULT/diff.txt > $RESULT/tmp; mv $RESULT/tmp $RESULT/diff.txt 

        if [ -s $RESULT/diff.txt ] 
        then
            echo "      KAR extracting content differs from legacy...test failed"
            cat $RESULT/diff.txt
            cleanup
            exit
        fi
    fi

    chmod -R +w $RESULT/o_extracted
    cleanup
}



test_cOld_txNew_cmp ()
{
    echo "Testing legacy archive - list and extract with new..."
    SS="simple_source"

    mkdir $RESULT

    # create new source and remove aliases legacy kar doesnt handle
    cp -r $INPUT $SS
    rm -rf $SS/alias_subdir $SS/local_f_subdir_alias

    $OLDKAR -f -c $ARCHIVE -d $SS
    
    if ! $KAR -t $ARCHIVE > $RESULT/nlist.txt
    then
        echo "      KAR could not test the archive...failed"
        rm -rf $SS
        cleanup
        exit
    fi

    if ! $KAR -l -t $ARCHIVE > $RESULT/nllist.txt
    then
        echo "      KAR could not test the archive with longlist...failed"
        rm -rf $SS
        cleanup
        exit
    fi

    if ! $KAR -x $ARCHIVE -d $RESULT/n_extracted
    then
        echo "      KAR could not extract the archive...failed"
        rm -rf $SS
        cleanup
        exit
    fi


    #start comparing outputs
    echo "   Comparing list outputs..."
    $OLDKAR -t $ARCHIVE > $RESULT/olist.txt
    if ! diff -w $RESULT/nlist.txt $RESULT/olist.txt > $RESULT/diff.txt
    then
        echo "      KAR listing differs from legacy...test failed"
        cat $RESULT/diff.txt
        cleanup
        exit
    fi

    # there is no purpose in testing long list mode. 
    # most of the differences are an attempt to correct errors in the old tool

    echo "   Comparing extracted archives..."
    $OLDKAR -x $ARCHIVE -d $RESULT/o_extracted
    if ! diff -q -r $RESULT/n_extracted $RESULT/o_extracted 2> $RESULT/diff.txt
    then
        grep -v "alias" $RESULT/diff.txt > $RESULT/tmp; mv $RESULT/tmp $RESULT/diff.txt 

        if [ -s $RESULT/diff.txt ] 
        then
            echo "      KAR extracting content differs from legacy...test failed"
            cat $RESULT/diff.txt
            cleanup
            exit
        fi
    fi

    chmod -R +w $RESULT/o_extracted
    rm -rf $SS
    cleanup
}


run_basic ()
{
    test_create
    test_create_options
    test_list
    test_list_options
    test_extract
}

run_compare ()
{
    test_cNew_txOld_cmp
    test_cOld_txNew_cmp
}


# run the script
run_basic
run_compare


cleanup


exit $STATUS
