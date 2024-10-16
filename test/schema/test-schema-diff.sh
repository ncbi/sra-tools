#!/bin/sh

test_schema_diff=$1
TOP=$2
OBJDIR=$3

echo Starting Schema Diff test

mkdir -p $OBJDIR/data

output=$(VDB_CONFIG=local.kfg $test_schema_diff -I$TOP/interfaces -o$OBJDIR/data \
	$TOP/interfaces/insdc/insdc.vschema \
	$TOP/interfaces/insdc/seq.vschema \
	$TOP/interfaces/insdc/sra.vschema \
	$TOP/interfaces/ncbi/ncbi.vschema \
	$TOP/interfaces/ncbi/seq.vschema \
	$TOP/interfaces/ncbi/spotname.vschema \
	$TOP/interfaces/ncbi/sra.vschema \
	$TOP/interfaces/ncbi/stats.vschema \
	$TOP/interfaces/ncbi/seq-graph.vschema \
	$TOP/interfaces/ncbi/wgs-contig.vschema \
	$TOP/interfaces/ncbi/clip.vschema \
	$TOP/interfaces/ncbi/varloc.vschema \
	$TOP/interfaces/ncbi/pnbrdb.vschema \
	$TOP/interfaces/vdb/built-in.vschema \
	$TOP/interfaces/vdb/vdb.vschema \
	$TOP/interfaces/align/mate-cache.vschema \
	$TOP/interfaces/align/refseq.vschema \
	$TOP/interfaces/align/seq.vschema \
	$TOP/interfaces/align/qstat.vschema \
	$TOP/interfaces/align/align.vschema \
	$TOP/interfaces/align/pileup-stats.vschema \
	$TOP/interfaces/sra/454.vschema \
	$TOP/interfaces/sra/ion-torrent.vschema \
	$TOP/interfaces/sra/illumina.vschema \
	$TOP/interfaces/sra/abi.vschema \
	$TOP/interfaces/sra/helicos.vschema \
	$TOP/interfaces/sra/nanopore.vschema \
	$TOP/interfaces/sra/generic-fastq.vschema \
	$TOP/interfaces/sra/pacbio.vschema \
	$TOP/interfaces/csra2/stats.vschema \
	$TOP/interfaces/csra2/read.vschema \
	$TOP/interfaces/csra2/reference.vschema \
	$TOP/interfaces/csra2/csra2.vschema
)

res=$?
if [ "$res" != "0" ];
	then echo "Schema Diff test FAILED, res=$res output=$output" && exit 1;
fi

echo Schema Diff test is finished


