#!/bin/bash

BINDIR=$1
VDB_LIBDIR=$2
WORKDIR=$3

echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' > ${WORKDIR}/tmp.mkfg
echo '/sra/quality_type = "raw_scores"' >>${WORKDIR}/tmp.mkfg

VDB_LIBRARY_PATH=${VDB_LIBDIR} python3 generate-test-data.py ${WORKDIR}/test-data || exit 1
${BINDIR}/kar -d ${WORKDIR}/test-data -fc test-data.kar || exit 2
rm -rf ${WORKDIR}/test-data expected

${BINDIR}/kar -d ${WORKDIR}/test-data -x test-data.kar || exit 3
${BINDIR}/make-read-filter --temp ${WORKDIR} ${WORKDIR}/test-data || exit 4
${BINDIR}/vdb-dump ${WORKDIR}/test-data >${WORKDIR}/actual || exit 5
${BINDIR}/kdbmeta -u ${WORKDIR}/test-data -T SEQUENCE STATS READ_FILTER_CHANGES >${WORKDIR}/actual.stats || exit 6
diff expected ${WORKDIR}/actual || exit 7
diff expected.stats ${WORKDIR}/actual.stats || exit 8
rm -rf ${WORKDIR}/test-data
