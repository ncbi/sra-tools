#! /bin/env bash

ACC="SRR25616912"
SRC="/mnt/data/SRA/ACC/TABLES_flat/$ACC"
DST="SRR25616912.sra.vdbcache"
TOOL="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin/sra-split"

rm -rf $DST
$TOOL --src $SRC --dst $DST
vdb-dump $DST -r
