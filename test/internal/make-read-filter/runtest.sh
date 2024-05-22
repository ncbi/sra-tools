#!/bin/sh

echo $*

BINDIR=$1
VDB_LIBDIR=$2
WORKDIR=$3
BIN_SUFFIX=$4

EXE="${BINDIR}/make-read-filter${BIN_SUFFIX}"
if ! test -f $EXE; then
    echo "$EXE does not exist. Skipping the test."
    exit 0
fi

echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' > ${WORKDIR}/tmp.mkfg
echo '/sra/quality_type = "raw_scores"' >>${WORKDIR}/tmp.mkfg

${BINDIR}/kar${BIN_SUFFIX} -d ${WORKDIR}/test-data -x test-data.kar || exit 1
${EXE} --temp ${WORKDIR} ${WORKDIR}/test-data || exit 2
${BINDIR}/vdb-dump${BIN_SUFFIX} ${WORKDIR}/test-data >${WORKDIR}/actual || exit 3
${BINDIR}/kdbmeta${BIN_SUFFIX} -u ${WORKDIR}/test-data -T SEQUENCE STATS READ_FILTER_CHANGES >${WORKDIR}/actual.stats || exit 4
diff expected ${WORKDIR}/actual || exit 5
diff expected.stats ${WORKDIR}/actual.stats || exit 6
rm -rf ${WORKDIR}/test-data
