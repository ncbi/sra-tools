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

KAR=kar
[ $# -ge 1 ] && KAR="$1"

ACCLIST="acc.txt"
acc=""

cleanup ()
{
    chmod -R +w $acc*
    rm -rf $acc* *-dump.txt
}

trap 'echo " quitting" && cleanup && exit 1' 2

for acc in `shuf -n 1 $ACCLIST | cut -d',' -f2`
do
    # extract an SRR with the new tool
    echo "extracting $acc"
    if ! kar -x `srapath $acc` -d $acc
    then
        STATUS=$?
        echo "new KAR failed to extract an exisiting SRR file"
        cleanup
        exit $STATUS
    fi
    
    # re-kar the exracted SRR to run VDB-DUMP
    echo "archiving $acc"
    if ! kar -c $acc.sra -d $acc
    then
        STATUS=$?
        echo "new KAR to create archive with previously extracted SRR file"
        cleanup
        exit $STATUS
    fi

    # vdb-dump the new archive and compare outputs from the old SRR archive
    echo "starting dumps"
    vdb-dump $acc.sra > new-$acc-dump.txt
    echo "   finished dumping new-$acc-dump.txt"
    vdb-dump `srapath $acc` > old-$acc-dump.txt
    echo "   finished dumping old-$acc-dump.txt"
    
    echo "comparing new-$acc-dump.txt and old-$acc-dump.txt"
    if ! diff new-$acc-dump.txt old-$acc-dump.txt
    then
        STATUS=$?
        echo "vdb-dump differs in new archive"
        cleanup
        exit $STATUS
    fi

    cleanup
done

exit $STATUS
