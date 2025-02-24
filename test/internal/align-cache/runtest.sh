#!/bin/sh

VDB_INCDIR=$1
DIRTOTEST=$2
tool_binary=$3

echo "testing ${tool_binary}"

if ! test -f ${DIRTOTEST}/${tool_binary}; then
    echo "${DIRTOTEST}/${tool_binary} does not exist. Skipping the test."
    exit 0
fi

rm -rf CSRA_file.cache
echo "vdb/schema/paths = \"${VDB_INCDIR}\"" > tmp.kfg
output=$(VDB_CONFIG=`pwd` ${DIRTOTEST}/${tool_binary} \
                           -t 10 --min-cache-count 1 CSRA_file CSRA_file.cache)
res=$?
if [ "$res" != "0" ];
	then echo "${tool_binary} FAILED, res=$res output=$output" && exit 1;
fi
${DIRTOTEST}/vdb-validate CSRA_file.cache/ 2>&1 | \
    grep -q "is consistent" || exit 1
rm tmp.kfg
rm -rf CSRA_file.cache
