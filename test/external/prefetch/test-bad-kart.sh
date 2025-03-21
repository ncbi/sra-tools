#!/usr/bin/env bash

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

if [ $# -lt 2 ]; then
  echo "Usage $0 <bin-dir> <tool-name>"
  exit 1
fi

bin_dir=$1
prefetch=$2
tool=$1/$2

echo "testing processing of a bad kart file via $tool"

if [ "" != "" ]; then
    perl -e 'print $ENV{"dbgap|110600"};print "=dbgap-110600\n"'
    perl -e 'print $ENV{"dbgap|110601"};print "=dbgap-110601\n"'
    perl -e 'print $ENV{"dbgap|110602"};print "=dbgap-110602\n"'
    perl -e 'print $ENV{"dbgap|110603"};print "=dbgap-110603\n"'
fi

i=0; while [ $i -le 3 ]; do
# echo "dbgap|11060$i"
  E=$(perl -e "print \$ENV{'dbgap|11060$i'}")
  if [ "$E" == "" ]; then
    echo "dbgap|11060$i env.var. is not set"
    i=$((i+2))
    exit $i
    fi
  i=$((i+1))
done

mkdir -p actual || exit 4

$tool --ngc data/prj_phs710EA_test.ngc data/bad-kart.krt > actual/out \
                                                        2> actual/err
res=$?
if [ $res -eq 0 ] ; then
  echo "prefetch of a bad kart file succeed when failure was expected"
  exit 7
fi

cat actual/out | perl -e'foreach(<>){s/.*: //;print}' > actual/outer
cat actual/err | perl -e \
     'foreach (<>) { next if /_ItemResolveResolved/; s/.*: //; print }' \
    > actual/errer
diff actual/outer expected/out || exit 8
diff actual/errer expected/err || exit 9

rm -r actual || exit 10

exit 0
