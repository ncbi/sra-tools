#!/bin/bash

VDB_INCDIR=$1
DIRTOTEST=$2

rm -rf CSRA_file.cache
echo "vdb/schema/paths = \"${VDB_INCDIR}\"" > tmp.kfg
VDB_CONFIG=`pwd` ${DIRTOTEST}/align-cache \
 	                          -t 10 --min-cache-count 1 CSRA_file CSRA_file.cache
${DIRTOTEST}/vdb-validate CSRA_file.cache/ 2>&1 \
| grep --quiet "is consistent"
rm tmp.kfg
rm -rf CSRA_file.cache
