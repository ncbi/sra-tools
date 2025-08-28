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
# =============================================================================$

bin_dir=$1
prefetch=$2

PREFETCH=$bin_dir/$prefetch

echo Testing ${prefetch} to out file from ${bin_dir}...

rm -f tmpfile

unset NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE
$PREFETCH SRR053325  -otmpfile > /dev/null 2>&1
if [ "$?" = "0" ]; then
    echo "Downloading to outfile succeed"; exit 1
fi

rm -f tmpfile

NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= \
    $PREFETCH SRR053325  -otmpfile > /dev/null || exit 2
rm -f tmpfile || exit 3

echo test of ${prefetch} to out file succeed.
