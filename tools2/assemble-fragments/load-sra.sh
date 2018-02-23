#!/bin/sh

echo "Reading $1 ..."
sra2ir "$@" | general-loader --log-level=err --include=${INCLUDE:-include} --schema=${SCHEMA:-schema}/aligned-ir.schema.text --target=$$.IR
echo "Sorting ..."
reorder-ir $$.IR | general-loader --log-level=err --include=${INCLUDE:-include} --schema=${SCHEMA:-schema}/aligned-ir.schema.text --target=$$.sorted
rm -rf $$.IR

echo "Filtering ..."
filter-ir $$.sorted | general-loader --log-level=err --include=${INCLUDE:-include} --schema=${SCHEMA:-schema}/aligned-ir.schema.text --target=$$.filtered
rm -rf $$.sorted

echo "Generating contiguous regions ..."
summarize-pairs map $$.filtered | sort -k1,1 -k2n,2n -k3n,3n -k4,4 -k5n,5n -k6n,6n | summarize-pairs reduce -  | general-loader --log-level=err --include=${INCLUDE:-include} --schema=${SCHEMA:-schema}/aligned-ir.schema.text --target=$$.contigs

echo "Assigning fragment alignments ..."
assemble-fragments $$.filtered $$.contigs | general-loader --log-level=err --include=${INCLUDE:-include} --schema=${SCHEMA:-schema}/aligned-ir.schema.text --target=$$.result
rm -rf $$.filtered $$.contigs

echo "Packaging final result $1.result.vdb"
kar --force -d $$.result -c $1.result.vdb
