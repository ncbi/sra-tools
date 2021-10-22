#!/bin/bash

echo $*

BINDIR=$1
VDB_LIBDIR=$2
WORKDIR=$3

echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' > ${WORKDIR}/tmp.mkfg
echo '/sra/quality_type = "raw_scores"' >>${WORKDIR}/tmp.mkfg

${BINDIR}/kar -d ${WORKDIR}/test-data -x test-data.kar || exit 1
${BINDIR}/make-read-filter --temp ${WORKDIR} ${WORKDIR}/test-data || exit 2
${BINDIR}/vdb-dump ${WORKDIR}/test-data >${WORKDIR}/actual || exit 3
${BINDIR}/kdbmeta -u ${WORKDIR}/test-data -T SEQUENCE STATS READ_FILTER_CHANGES >${WORKDIR}/actual.stats || exit 4
diff expected ${WORKDIR}/actual || exit 5
diff expected.stats ${WORKDIR}/actual.stats || exit 6
rm -rf ${WORKDIR}/test-data
