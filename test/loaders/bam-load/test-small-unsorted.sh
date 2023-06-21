#!/bin/sh

BINDIR=${1:-BINDIR}
BAMLOAD=$(readlink -f ${BINDIR}/bam-load)
VDBDUMP=$(readlink -f ${BINDIR}/vdb-dump)

${BAMLOAD} db/small-unsorted.sam \
    --log-level err \
    --ref-file db/small-unsorted.fasta \
    --output small-unsorted.out || exit 1

${VDBDUMP} small-unsorted.out \
    --table PRIMARY_ALIGNMENT \
    --columns SPOT_COUNT \
    --rows 1 \
    --format tab > expected.small-unsorted
    
${VDBDUMP} small-unsorted.out \
    --table REFERENCE \
    --columns PRIMARY_ALIGNMENT_IDS \
    --numelem \
    --rows 1 \
    --format tab > actual.small-unsorted

diff -q expected.small-unsorted actual.small-unsorted >/dev/null
DIFF=${?}
rm -rf expected.small-unsorted actual.small-unsorted small-unsorted.out
exit ${DIFF}

