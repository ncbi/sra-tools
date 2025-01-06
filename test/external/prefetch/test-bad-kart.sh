#!/usr/bin/env bash

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
