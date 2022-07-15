#!/bin/bash

progname=`basename $0`
suffix=$1
old=$2
[ ! -z "$suffix" ] || { echo "Empty directory suffix parameter"; exit 1; }

test_list=(`ls -d *.${suffix} 2>/dev/null`)
for test in ${test_list[*]}
do
  if [[ $old == "old" ]]; then
    ./run_test_old.sh ${test}
  else
    ./run_test.sh ${test}
  fi  
  if [ $? -eq 0 ]; then 
      echo "${test} ${old} passed"
  else
      echo "${test} ${old} failed"
  fi
  
done


exit 0
