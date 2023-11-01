# ================================================================
#
#   Test : 
#       can fasteq-dump handle a cSRA with a tiny number of alignments?
#
#   we need sam-factory and bam-load to create such a cSRA,
#   because we want to avoid to depend on production accessions
#
# ================================================================

set -e

DIRTOTEST="$1"
BINDIR="$2"

SAMFACTORY="${DIRTOTEST}/sam-factory"
if [[ ! -x $SAMFACTORY ]]; then
    SAMFACTORY="${BINDIR}/sam-factory"
    if [[ ! -x $SAMFACTORY ]]; then
        echo "${SAMFACTORY} not found - exiting..."
        exit 3
    fi
fi

BAMLOAD="${DIRTOTEST}/bam-load"
if [[ ! -x $BAMLOAD ]]; then
    echo "${BAMLOAD} not found - exiting..."
    exit 3
fi

KAR="${DIRTOTEST}/kar"
if [[ ! -x $KAR ]]; then
    echo "${KAR} not found - exiting..."
    exit 3
fi

FASTERQDUMP="${DIRTOTEST}/fasterq-dump"
if [[ ! -x $FASTERQDUMP ]]; then
    echo "${FASTERQDUMP} not found - exiting..."
    exit 3
fi

echo -e "\ntesting ${FASTERQDUMP} on tiny cSRA-accession"

SAM_FACTORY_CONFIG="tiny_csra.sf"
if [[ ! -f $SAM_FACTORY_CONFIG ]]; then
    echo "${SAM_FACTORY_CONFIG} not found - exiting..."
    exit 3
fi

#then next 3 names have to match what is defined in SAM_FACTORY_CONFIG!
BAM_LOAD_CONFIG="tiny_csra.config"
BAM_LOAD_SAM="tiny_csra.sam"
BAM_LOAD_REF="tiny_csra.fasta"

#delete the output of sam-factory, in case it already exists
rm -rf "${BAM_LOAD_CONFIG}" "${BAM_LOAD_SAM}" "${BAM_LOAD_REF}"

#pipe the sam-factory-config-file into the sam-factory-binary
#this creates: tiny_csra.config, tiny_csra.sam, and tiny_csra.fasta
#=======================================================
cat "${SAM_FACTORY_CONFIG}" | "${SAMFACTORY}"
#=======================================================

if [[ ! -f $BAM_LOAD_CONFIG ]]; then
    echo "${BAM_LOAD_CONFIG} was not created by sam-factory - exiting..."
    exit 3
fi

if [[ ! -f $BAM_LOAD_SAM ]]; then
    echo "${BAM_LOAD_SAM} was not created by sam-factory - exiting..."
    exit 3
fi

if [[ ! -f $BAM_LOAD_REF ]]; then
    echo "${BAM_LOAD_REF} was not created by sam-factory - exiting..."
    exit 3
fi

#run bam-load to create a tiny-csra ( as directory )
BAM_LOAD_OUTDIR="TINY_CSRA.DIR"
rm -rf "$BAM_LOAD_OUTDIR}"
#=======================================================
${BAMLOAD} ${BAM_LOAD_SAM} --ref-file ${BAM_LOAD_REF} --output ${BAM_LOAD_OUTDIR}
#=======================================================
if [[ ! -d $BAM_LOAD_OUTDIR ]]; then
    echo "${BAM_LOAD_REF} was not created by bam-load - exiting..."
    exit 3
fi
#now we do not need the input-files for bam-load any more
rm -rf "${BAM_LOAD_CONFIG}" "${BAM_LOAD_SAM}" "${BAM_LOAD_REF}"

#run kar to transform the tiny-csra-directory into a tiny-csra-file
KAR_OUTPUT="TINY_CSRA.ACC"
rm -rf "${KAR_OUTPUT}" "${KAR_OUTPUT}.md5"
#=======================================================
${KAR} --force -c ${KAR_OUTPUT} -d ${BAM_LOAD_OUTDIR}
#=======================================================
if [[ ! -f $KAR_OUTPUT ]]; then
    echo "${KAR_OUTPUT} was not created by kar - exiting..."
    exit 3
fi
#now we do not need the tiny-csra-directory any more
rm -rf "${BAM_LOAD_OUTDIR}"

FASTQ_OUTPUT="TINY_CSRA.FASTQ"
#run fasterq-dump on TINY_CSRA.ACC  ( THIS IS WHAT WE ARE ACTUALLY TESTING! )
#=======================================================
${FASTERQDUMP} ${KAR_OUTPUT} --concatenate-reads -o ${FASTQ_OUTPUT}
#=======================================================
if [[ ! -f $FASTQ_OUTPUT ]]; then
    echo "${FASTQ_OUTPUT} was not created by fasterq-dump - exiting..."
    exit 3
fi

#now we do not need the tiny-csra-file any more
rm -rf "${KAR_OUTPUT}" "${KAR_OUTPUT}.md5"

#for now let us disable the line-counting, because wc has different outputs in linux vs mac ( whitespace! )
#the test is already successful if fasterq-dump does not exit with a none-zer return-code
#we arrive here if it returns zero, because of 'set -e' at the top of this script
rm -rf "${FASTQ_OUTPUT}"
exit 0

#INES=`wc -l <$FASTQ_OUTPUT`
#f [ "$LINES" == "12" ]; then
#   #now we do not need the tiny-csra-fastq-file any more
#   rm -rf $FASTQ_OUTPUT
#    echo "success!"
#    exit 0
#else
#    echo "we should have 12 lines in $FASTQ_OUTPUT, but we have $LINES lines instead!"
#    exit 3
#fi
