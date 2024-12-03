#!/bin/sh

bin_dir=$1

#perl -e 'print "NC_000005.8=";print $ENV{"NC_000005.8"};print"\n"'
#echo NCBI_SETTINGS=$NCBI_SETTINGS
#echo NCBI_VDB_NO_ETC_NCBI_KFG=$NCBI_VDB_NO_ETC_NCBI_KFG

#$bin_dir/vdb-config -on
#$bin_dir/srapath NC_000005.8

# Verify that phid and error message are printed
# when SDL fails to resolve RefSeq
$bin_dir/align-info SRR619505 2>&1 | grep -q "ncbi_phid='aHh'"
res=$?
if [ "$res" != "0" ] ; then
    echo check of refseq failure FAILED && exit 1
#else
#   echo SUCCESS
fi
