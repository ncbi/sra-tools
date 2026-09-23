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

TIME="/usr/bin/time -f \"%E real, %U user,%S sys\""
SEARCH="../../../build/cmake/Release/tools/sra-search/sra-search"
PATTERN="ACGTAGGGTCC"
SINGLE="SRR600094"
MULTIPLE="SRR600094 SRR600095 SRR600096 SRR600099"

lscpu
echo

echo "1 accession, 1 thread"
CMD="$TIME $SEARCH $PATTERN $SINGLE"
echo $CMD
eval $CMD  >/dev/null || exit 1

echo "thread per blob, 1 accession, 2 threads"
CMD="$TIME $SEARCH $PATTERN $SINGLE --threads 2 --blobs"
echo $CMD
eval $CMD  >/dev/null || exit 1

echo "thread per blob, 1 accession, 4 threads"
CMD="$TIME $SEARCH $PATTERN $SINGLE --threads 4 --blobs"
echo $CMD
eval $CMD  >/dev/null || exit 1

echo "4 accessions, 1 thread"
CMD="$TIME $SEARCH $PATTERN $MULTIPLE"
echo $CMD
eval $CMD  >/dev/null || exit 1

echo "thread per accession, 4 accessions, 2 threads"
CMD="$TIME $SEARCH $PATTERN $MULTIPLE --threads 2"
echo $CMD
eval $CMD  >/dev/null || exit 1

echo "thread per accession, 4 accessions, 4 threads"
CMD="$TIME $SEARCH $PATTERN $MULTIPLE --threads 4"
echo $CMD
eval $CMD  >/dev/null || exit 1

echo "thread per blob, 4 accessions, 2 threads"
CMD="$TIME $SEARCH $PATTERN $MULTIPLE --threads 2 --blobs"
echo $CMD
eval $CMD  >/dev/null || exit 1

echo "thread per blob, 4 accessions, 4 threads"
CMD="$TIME $SEARCH $PATTERN $MULTIPLE --threads 4 --blobs"
echo $CMD
eval $CMD  >/dev/null || exit 1

echo "thread per blob, 4 accessions, 6 threads"
CMD="$TIME $SEARCH $PATTERN $MULTIPLE --threads 6 --blobs"
echo $CMD
eval $CMD  >/dev/null || exit 1
